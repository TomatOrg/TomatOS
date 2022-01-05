#include <util/string.h>
#include <arch/intrin.h>
#include <sync/spinlock.h>
#include <kernel.h>
#include <arch/msr.h>
#include "vmm.h"
#include "early.h"

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
        case STIVALE2_MMAP_USABLE: return "usable";
        case STIVALE2_MMAP_RESERVED: return "reserved";
        case STIVALE2_MMAP_ACPI_RECLAIMABLE: return "ACPI reclaimable";
        case STIVALE2_MMAP_ACPI_NVS: return "ACPI NVS";
        case STIVALE2_MMAP_BAD_MEMORY: return "bad memory";
        case STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE: return "bootloader reclaimable";
        case STIVALE2_MMAP_KERNEL_AND_MODULES: return "kernel/modules";
        case STIVALE2_MMAP_FRAMEBUFFER: return "framebuffer";
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

    // we need the kernel segments and base to properly map ourselves
    struct stivale2_struct_tag_pmrs* pmrs = get_stivale2_tag(STIVALE2_STRUCT_TAG_PMRS_ID);
    struct stivale2_struct_tag_kernel_base_address* kbase = get_stivale2_tag(STIVALE2_STRUCT_TAG_KERNEL_BASE_ADDRESS_ID);
    struct stivale2_struct_tag_memmap* memmap = get_stivale2_tag(STIVALE2_STRUCT_TAG_MEMMAP_ID);
    CHECK_ERROR(pmrs != NULL, ERROR_NOT_FOUND);
    CHECK_ERROR(kbase != NULL, ERROR_NOT_FOUND);
    CHECK_ERROR(memmap != NULL, ERROR_NOT_FOUND);

    // map all the physical memory nicely, this will not include the memory used to
    // actually create the the page table (at least part of it), but that is because
    // of how the early memory allocator works...
    TRACE("Memory mapping:");
    for (int i = 0; i < memmap->entries; i++) {
        struct stivale2_mmap_entry* entry = &memmap->memmap[i];
        uintptr_t base = entry->base;
        size_t length = entry->length;
        int type = entry->type;
        uintptr_t end = base + length;
        const char* name = get_memmap_type_name(type);

        // don't map bad memory
        if (type != STIVALE2_MMAP_BAD_MEMORY) {
            // now map it to the direct map
            base = ALIGN_DOWN(base, PAGE_SIZE);
            end = ALIGN_UP(end, PAGE_SIZE);
            map_perm_t perms = 0;
            if (
                type == STIVALE2_MMAP_USABLE ||
                type == STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE ||
                type == STIVALE2_MMAP_FRAMEBUFFER
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
    for (int i = 0; i < pmrs->entries; i++) {
        struct stivale2_pmr* pmr = &pmrs->pmrs[i];

        // get the physical base
        size_t offset = pmr->base - kbase->virtual_base_address;
        uintptr_t phys = kbase->physical_base_address + offset;

        // make sure it is properly aligned
        CHECK((phys % PAGE_SIZE) == 0, "Physical base is not aligned (%p)", phys);
        CHECK((pmr->base % PAGE_SIZE) == 0, "Base is not aligned (%p)", pmr->base);
        CHECK((pmr->length % PAGE_SIZE) == 0, "Length is not aligned (%p)", pmr->length);

        // Log it
        char r = pmr->permissions & STIVALE2_PMR_READABLE ? 'r' : '-';
        char w = pmr->permissions & STIVALE2_PMR_WRITABLE ? 'w' : '-';
        char x = pmr->permissions & STIVALE2_PMR_EXECUTABLE ? 'x' : '-';
        TRACE("\t%016p-%016p (%08p-%08p) [%c%c%c]",
              pmr->base, pmr->base + pmr->length,
              phys, phys + pmr->length,
              r, w, x);

        // actually map it
        map_perm_t perms = MAP_UNMAP_DIRECT;
        if (pmr->permissions & STIVALE2_PMR_WRITABLE) perms |= MAP_WRITE;
        if (pmr->permissions & STIVALE2_PMR_EXECUTABLE) perms |= MAP_EXEC;
        CHECK_AND_RETHROW(vmm_map(phys, (void*)pmr->base, pmr->length / PAGE_SIZE, perms));
    }

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

/**
 * Unmap a single page from the direct map, don't page fault
 * if the page is not already mapped.
 *
 * @param pa    [IN] the physical page to unmap
 */
static void vmm_unmap_direct_page(uintptr_t pa) {
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
static uintptr_t vmm_alloc_page() {
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

/**
 * Setup a single level of the page table, this will allocate the page
 * if needed
 *
 * @param pml       [IN] The PML virtual base
 * @param index     [IN] The index of the page in the level
 */
static bool vmm_setup_level(page_entry_t* pml, page_entry_t* next_pml, size_t index) {
    if (!pml[index].present) {
        uintptr_t frame = vmm_alloc_page();
        if (frame == INVALID_PHYS_ADDR) {
            return false;
        }

        // map it
        pml[index] = (page_entry_t) {
            .present = 1,
            .frame = frame >> 12,
            .writeable = 1
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

    for (uintptr_t cva = (uintptr_t)va; cva < (uintptr_t)va + page_count * PAGE_SIZE; cva += PAGE_SIZE, pa += PAGE_SIZE) {
        // calculate the indexes of each of these
        size_t pml4i = (cva >> 39) & 0x1FFull;
        size_t pml3i = (cva >> 30) & 0x3FFFFull;
        size_t pml2i = (cva >> 21) & 0x7FFFFFFull;
        size_t pml1i = (cva >> 12) & 0xFFFFFFFFFull;

        // setup the top levels properly
        CHECK_ERROR(vmm_setup_level(PAGE_TABLE_PML4, PAGE_TABLE_PML3, pml4i), ERROR_OUT_OF_RESOURCES);
        CHECK_ERROR(vmm_setup_level(PAGE_TABLE_PML3, PAGE_TABLE_PML2, pml3i), ERROR_OUT_OF_RESOURCES);
        CHECK_ERROR(vmm_setup_level(PAGE_TABLE_PML2, PAGE_TABLE_PML1, pml2i), ERROR_OUT_OF_RESOURCES);

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

err_t vmm_alloc(void* va, size_t page_count, map_perm_t perms) {
    err_t err = NO_ERROR;
    void* cva = NULL;

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

        TRACE("Stack allocation %p", fault_address);

        // we are good, map the page
        CHECK_AND_RETHROW(vmm_alloc((void*) ALIGN_DOWN(fault_address, PAGE_SIZE), 1, MAP_WRITE | MAP_UNMAP_DIRECT));
    } else {
        CHECK_FAIL("Invalid paging request at %p", fault_address);
    }

cleanup:
    return err;
}
