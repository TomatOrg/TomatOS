#include "vmm.h"

#include <sync/spinlock.h>

#include <util/string.h>
#include <util/elf64.h>

#include <arch/apic.h>
#include <arch/msr.h>

#include <kernel.h>

#include "mem.h"
#include "early.h"

#include "arch/intrin.h"

// The recursive page table addresses
#define PAGE_TABLE_PML1            ((page_entry_t*)0xFFFFFF0000000000ull)
#define PAGE_TABLE_PML2            ((page_entry_t*)0xFFFFFF7F80000000ull)
#define PAGE_TABLE_PML3            ((page_entry_t*)0xFFFFFF7FBFC00000ull)
#define PAGE_TABLE_PML4            ((page_entry_t*)0xFFFFFF7FBFDFE000ull)

/**
 * The root physical address of the kernel phys table
 */
static uintptr_t m_pml4_pa = INVALID_PHYS_ADDR;

/**
 * Should we use the early memory allocator
 */
static bool m_early_alloc = true;

/**
 * Get the name of the given stivale memory map entry type
 *
 * @param type  [IN] the stivale2 type
 */
static const char* get_memmap_type_name(uint32_t type) {
    switch (type) {
        case LIMINE_MEMMAP_USABLE: return "usable";
        case LIMINE_MEMMAP_RESERVED: return "reserved";
        case LIMINE_MEMMAP_ACPI_RECLAIMABLE: return "ACPI reclaimable";
        case LIMINE_MEMMAP_ACPI_NVS: return "ACPI NVS";
        case LIMINE_MEMMAP_BAD_MEMORY: return "bad memory";
        case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE: return "bootloader reclaimable";
        case LIMINE_MEMMAP_KERNEL_AND_MODULES: return "kernel/modules";
        case LIMINE_MEMMAP_FRAMEBUFFER: return "framebuffer";
        default: return NULL;
    }
}

err_t init_vmm() {
    err_t err = NO_ERROR;

    // Setup recursive paging, we are going to set this for both the new cr3 and
    // the current (bootloader provided) cr3 so we can modify the actual cr3 using
    // our current address space
    m_pml4_pa = early_alloc_page_phys();
    page_entry_t* new_pml4 = PHYS_TO_DIRECT(m_pml4_pa);
    new_pml4[510] = (page_entry_t) {
        .present = 1,
        .writeable = 1,
        .frame = m_pml4_pa >> 12
    };

    // set the 510th entry of the current cr3
    page_entry_t* current_pml4 = PHYS_TO_DIRECT(__readcr3());
    current_pml4[510] = new_pml4[510];

    // map all the physical memory nicely, this will not include the memory used to
    // actually create the the page table (at least part of it), but that is because
    // of how the early memory allocator works...
    TRACE("Memory mapping:");
    for (int i = 0; i < g_limine_memmap.response->entry_count; i++) {
        struct limine_memmap_entry* entry = g_limine_memmap.response->entries[i];
        uintptr_t base = entry->base;
        size_t length = entry->length;
        int type = entry->type;
        uintptr_t end = base + length;
        const char* name = get_memmap_type_name(type);

        // don't map bad memory
        if (type != LIMINE_MEMMAP_BAD_MEMORY) {
            // now map it to the direct map
            base = ALIGN_DOWN(base, PAGE_SIZE);
            end = ALIGN_UP(end, PAGE_SIZE);
            map_perm_t perms = 0;
            if (
                type == LIMINE_MEMMAP_USABLE ||
                type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE ||
                type == LIMINE_MEMMAP_FRAMEBUFFER
            ) {
                // these pages we are going to map as rw, the rest we are going to map
                // as read only
                perms = MAP_WRITE;
            }

            if (name != NULL) {
                TRACE("\t%p-%p (%08p-%08p) [r%c-]: %s",
                      entry->base, entry->base + entry->length,
                      PHYS_TO_DIRECT(base), PHYS_TO_DIRECT(end),
                      perms & MAP_WRITE ? 'w' : '-',
                      name);
            } else {
                TRACE("\t%p-%p (%08p-%08p) [r--]: <unknown type %04x>",
                      entry->base, entry->base + entry->length,
                      PHYS_TO_DIRECT(base), PHYS_TO_DIRECT(end),
                      type);
            }

            CHECK_AND_RETHROW(vmm_map(base, PHYS_TO_DIRECT(base), (end - base) / PAGE_SIZE, perms));
        } else {
            // these ranges we are not going to map at all
            TRACE("\t%p-%p (unmapped) [---]: %s", entry->base, entry->base + entry->length, name);
        }
    }

    // Map the kernel properly, we are going to remove it from the direct map
    // just in case
    TRACE("Kernel mapping:");
    void* kernel = g_limine_kernel_file.response->kernel_file->address;
    Elf64_Ehdr* ehdr = kernel;
    CHECK(ehdr->e_phoff != 0);

    // get the tables we need
    Elf64_Phdr* phdrs = kernel + ehdr->e_phoff;
    for (int i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr* phdr = &phdrs[i];
        if (phdr->p_type == PT_LOAD) {

            // Make sure this is actually in the kernel, otherwise
            // we might try to map the cpu locals
            if (phdr->p_vaddr < g_limine_kernel_address.response->virtual_base) {
                continue;
            }

            // get the physical base
            size_t offset = phdr->p_vaddr - g_limine_kernel_address.response->virtual_base;
            uintptr_t phys = g_limine_kernel_address.response->physical_base + offset;
            size_t aligned_size = ALIGN_UP(phdr->p_memsz, PAGE_SIZE);

            // make sure it is properly aligned
            CHECK((phys % PAGE_SIZE) == 0, "Physical address is not aligned (%p)", phys);
            CHECK((phdr->p_vaddr % PAGE_SIZE) == 0, "Virtual address is not aligned (%p)", phdr->p_vaddr);
            CHECK((aligned_size % PAGE_SIZE) == 0, "Memory aligned_size is not aligned (%p)", aligned_size);

            // Log it
            char r = phdr->p_flags & PF_R ? 'r' : '-';
            char w = phdr->p_flags & PF_W ? 'w' : '-';
            char x = phdr->p_flags & PF_X ? 'x' : '-';
            TRACE("\t%016p-%016p (%08p-%08p) [%c%c%c]",
                  phdr->p_vaddr, phdr->p_vaddr + phdr->p_memsz,
                  phys, phys + aligned_size,
                  r, w, x);

            // actually map it
            map_perm_t perms = MAP_UNMAP_DIRECT;
            if (phdr->p_flags & PF_W) perms |= MAP_WRITE;
            if (phdr->p_flags & PF_X) perms |= MAP_EXEC;
            CHECK_AND_RETHROW(vmm_map(phys, (void*)phdr->p_vaddr, aligned_size / PAGE_SIZE, perms));
        }
    }

    // map everything else that needs to be mapped at
    // this point
    CHECK_AND_RETHROW(init_apic());

    // setup everything on this cpu
    init_vmm_per_cpu();

cleanup:
    return err;
}

void vmm_switch_allocator() {
    m_early_alloc = false;
}

void init_vmm_per_cpu() {
    // set the NX bit, disable syscall since we are not gonna use them
    msr_efer_t efer = (msr_efer_t) { .packed = __readmsr(MSR_IA32_EFER) };
    efer.NXE = 1;
    efer.SCE = 0;
    __writemsr(MSR_IA32_EFER, efer.packed);

    // set the phys table for the current CPU
    __writecr3(m_pml4_pa);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Implementation details of the vmm
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void vmm_unmap_direct_page(uintptr_t pa) {
    uintptr_t va = (uintptr_t)PHYS_TO_DIRECT(pa);
    size_t pml4i = (va >> 39) & 0x1FFull;
    size_t pml3i = (va >> 30) & 0x3FFFFull;
    size_t pml2i = (va >> 21) & 0x7FFFFFFull;
    size_t pml1i = (va >> 12) & 0xFFFFFFFFFull;
    if (!PAGE_TABLE_PML4[pml4i].present) return;
    if (!PAGE_TABLE_PML3[pml3i].present) return;
    if (!PAGE_TABLE_PML2[pml2i].present) return;
    if (!PAGE_TABLE_PML1[pml1i].present) return;
    PAGE_TABLE_PML1[pml1i] = (page_entry_t){ 0 };
    __invlpg((void*)va);
}

/**
 * Allocate a page for the vmm to use
 */
uintptr_t vmm_alloc_page() {
    uintptr_t new_phys = INVALID_PHYS_ADDR;

    if (m_early_alloc) {
        // before we are done with the palloc init we need to still be able
        // to map virtual memory, for that we can use the early alloc
        new_phys = early_alloc_page_phys();

    } else {
        // after the early boot we can use palloc properly
        void* page = palloc(PAGE_SIZE);
        if (page == NULL) {
            return INVALID_PHYS_ADDR;
        }
        new_phys = DIRECT_TO_PHYS(page);
    }

    // unmap from the direct map
    vmm_unmap_direct_page(new_phys);

    // return the new address
    return new_phys;
}

bool vmm_setup_level(page_entry_t* pml, page_entry_t* next_pml, size_t index) {
    if (!pml[index].present) {
        uintptr_t frame = vmm_alloc_page();
        if (frame == INVALID_PHYS_ADDR) {
            return false;
        }

        // map it
        pml[index] = (page_entry_t) {
            .present = 1,
            .writeable = 1,
            .frame = frame >> 12
        };

        // now that it is mapped we can clear it
        void* page = (uint8_t*)next_pml + index * PAGE_SIZE;
        __invlpg(page);
        memset(page, 0, PAGE_SIZE);
    }

    return true;
}

err_t vmm_map(uintptr_t pa, void* va, size_t page_count, map_perm_t perms) {
    err_t err = NO_ERROR;

    CHECK(((uintptr_t)va % 4096) == 0);
    CHECK(((uintptr_t)pa % 4096) == 0);

    for (uintptr_t cva = (uintptr_t)va; cva < (uintptr_t)va + page_count * PAGE_SIZE; cva += PAGE_SIZE, pa += PAGE_SIZE) {
        // calculate the indexes of each of these
        size_t pml4i = (cva >> 39) & 0x1FFull;
        size_t pml3i = (cva >> 30) & 0x3FFFFull;
        size_t pml2i = (cva >> 21) & 0x7FFFFFFull;
        size_t pml1i = (cva >> 12) & 0xFFFFFFFFFull;

        // setup the top levels properly
        CHECK_ERROR(vmm_setup_level(PAGE_TABLE_PML4, PAGE_TABLE_PML3, pml4i), ERROR_OUT_OF_MEMORY);
        CHECK_ERROR(vmm_setup_level(PAGE_TABLE_PML3, PAGE_TABLE_PML2, pml3i), ERROR_OUT_OF_MEMORY);
        CHECK_ERROR(vmm_setup_level(PAGE_TABLE_PML2, PAGE_TABLE_PML1, pml2i), ERROR_OUT_OF_MEMORY);

        // setup the pml1 entry
        PAGE_TABLE_PML1[pml1i] = (page_entry_t) {
            .present = 1,
            .frame = pa >> 12,
            .writeable = (perms & MAP_WRITE) ? 1 : 0,
            .no_execute = (perms & MAP_EXEC) ? 0 : 1,
        };

        // invalidate the new mapped address
        __invlpg((void*)cva);

        // unmap the direct page if we need to
        if (perms & MAP_UNMAP_DIRECT) {
            vmm_unmap_direct_page(pa);
        }
    }

cleanup:
    return err;
}

err_t vmm_set_perms(void* va, size_t page_count, map_perm_t perms) {
    err_t err = NO_ERROR;

    CHECK(((uintptr_t)va % 4096) == 0);

    // simply iterate all the indexes and free them
    for (int i = 0; i < page_count; i++, va += PAGE_SIZE) {
        size_t pml1i = ((uintptr_t)va >> 12) & 0xFFFFFFFFFull;

        // make sure the page is mapped and change the write/exec perms
        CHECK(PAGE_TABLE_PML1[pml1i].present);
        PAGE_TABLE_PML1[pml1i].writeable = (perms & MAP_WRITE) ? 1 : 0;
        PAGE_TABLE_PML1[pml1i].no_execute = (perms & MAP_EXEC) ? 0 : 1;

        // unmap if needed
        if (perms & MAP_UNMAP_DIRECT) {
            vmm_unmap_direct_page(PAGE_TABLE_PML1[pml1i].frame << 12);
        }

        // invalidate the TLB entry
        // TODO: shootdown for the other cores
        __invlpg(va);
    }

cleanup:
    return err;
}

err_t vmm_alloc(void* va, size_t page_count, map_perm_t perms) {
    err_t err = NO_ERROR;
    void* cva = NULL;

    CHECK(((uintptr_t)va % 4096) == 0);

    for (cva = va; cva < va + page_count * PAGE_SIZE; cva += PAGE_SIZE) {
        uintptr_t page = vmm_alloc_page();
        CHECK(page != INVALID_PHYS_ADDR);
        CHECK_AND_RETHROW(vmm_map(page, cva, 1, perms));
    }

cleanup:
    // TODO: proper cleanup of this function

    return err;
}

void vmm_unmap(void* va, size_t page_count, uintptr_t* phys) {

    size_t pml1i = ((uintptr_t)va >> 12) & 0xFFFFFFFFFull;

    // simply iterate all the indexes and free them
    for (int i = 0; i < page_count; i++, pml1i++) {
        if (PAGE_TABLE_PML1[pml1i].present) {
            if (phys != NULL) {
                phys[i] = PAGE_TABLE_PML1[pml1i].frame << 12;
            }
            PAGE_TABLE_PML1[pml1i] = (page_entry_t){ 0 };
        } else {
            if (phys != NULL) {
                phys[i] = INVALID_PHYS_ADDR;
            }
        }
    }

}

err_t vmm_page_fault_handler(uintptr_t fault_address, bool write, bool present) {
    err_t err = NO_ERROR;

    if (KERNEL_HEAP_START <= fault_address && fault_address < KERNEL_HEAP_END) {
        // make sure this happens only for non-present page
        CHECK(!present);

        // on-demand kernel heap, just alloc it
        CHECK_AND_RETHROW(vmm_alloc((void*) ALIGN_DOWN(fault_address, PAGE_SIZE), 1, MAP_WRITE | MAP_UNMAP_DIRECT));

    } else if (STACK_POOL_START <= fault_address && fault_address < STACK_POOL_END) {
        // make sure this happens only for non-present page
        CHECK(!present);

        // check if this is a guard page, the first 2 1mb pags are the actual stack and the last 1mb page
        // is the stack guard
        uintptr_t index = (ALIGN_DOWN(fault_address - STACK_POOL_START, SIZE_1MB) / SIZE_1MB) % 3;
        CHECK(index != 0, "Tried to access stack guard page (index=%d)", index);

        // we are good, map the page
        CHECK_AND_RETHROW(vmm_alloc((void*) ALIGN_DOWN(fault_address, PAGE_SIZE), 1, MAP_WRITE | MAP_UNMAP_DIRECT));
    } else {
        CHECK_FAIL("Invalid paging request at %p", fault_address);
    }

cleanup:
    return err;
}
