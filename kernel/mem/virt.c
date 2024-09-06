#include "lib/elf64.h"
#include "virt.h"

#include <arch/regs.h>

#include "arch/intrin.h"
#include "limine.h"
#include "memory.h"
#include "phys.h"
#include "sync/spinlock.h"
#include "lib/string.h"


LIMINE_REQUEST struct limine_kernel_address_request g_limine_kernel_address_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST
};

LIMINE_REQUEST struct limine_hhdm_request g_limine_hhdm_request = {
    .id = LIMINE_HHDM_REQUEST
};

LIMINE_REQUEST struct limine_kernel_file_request g_limine_kernel_file_request = {
    .id = LIMINE_KERNEL_FILE_REQUEST
};

/**
 * The virtual base of the kernel
 */
static uintptr_t m_kernel_virtual_base = 0;

/**
 * The physical base of the kernel
 */
static uintptr_t m_kernel_physical_base = 0;

/**
 * Spinlock for mapping virtual pages
 */
static spinlock_t m_virt_lock = INIT_SPINLOCK();

/**
 * The kernel top level cr3
 */
static page_entry_t* m_cr3 = 0;

err_t init_virt_early() {
    err_t err = NO_ERROR;

    // get the base and address
    CHECK(g_limine_kernel_address_request.response != NULL);
    m_kernel_virtual_base = g_limine_kernel_address_request.response->virtual_base;
    m_kernel_physical_base = g_limine_kernel_address_request.response->physical_base;

    // make sure the kernel is at the correct virtual address
    CHECK(m_kernel_virtual_base >= 0xFFFFFFFF800000);

    // make sure the HHDM is at the correct address
    CHECK(g_limine_hhdm_request.response != NULL);
    CHECK(g_limine_hhdm_request.response->offset == DIRECT_MAP_OFFSET);

cleanup:
    return err;
}

static page_entry_t* get_next_level(page_entry_t* entry) {
    if (!entry->present) {
        void* phys = phys_alloc(PAGE_SIZE);
        if (phys == NULL) {
            return NULL;
        }
        memset(phys, 0, SIZE_4KB);

        entry->present = 1;
        entry->writeable = 1;
        entry->frame = DIRECT_TO_PHYS(phys) >> 12;
    }

    return PHYS_TO_DIRECT(entry->frame << 12);
}

err_t virt_map_page(uint64_t phys, uintptr_t virt, map_flags_t flags) {
    err_t err = NO_ERROR;

    spinlock_lock(&m_virt_lock);

    page_entry_t* pml3 = get_next_level(&m_cr3[PML4_INDEX(virt)]);
    CHECK_ERROR(pml3 != NULL, ERROR_OUT_OF_MEMORY);

    page_entry_t* pml2 = get_next_level(&pml3[PML3_INDEX(virt)]);
    CHECK_ERROR(pml2 != NULL, ERROR_OUT_OF_MEMORY);

    page_entry_t* pml1 = get_next_level(&pml2[PML2_INDEX(virt)]);
    CHECK_ERROR(pml1 != NULL, ERROR_OUT_OF_MEMORY);

    pml1[PML1_INDEX(virt)] = (page_entry_t){
        .present = 1,
        .writeable = (flags & MAP_PERM_W) != 0,
        .no_execute = (flags & MAP_PERM_X) == 0,
        .frame = phys >> 12
    };

cleanup:
    spinlock_unlock(&m_virt_lock);

    return err;
}

err_t virt_map_range(uint64_t phys, uintptr_t virt, size_t page_count, map_flags_t flags) {
    err_t err = NO_ERROR;

    for (size_t i = 0; i < page_count; i++) {
        uintptr_t vaddr = virt + (i * SIZE_4KB);
        uintptr_t paddr = phys + (i * SIZE_4KB);
        RETHROW(virt_map_page(paddr, vaddr, flags));
    }

cleanup:
    return err;
}

err_t init_virt() {
    err_t err = NO_ERROR;

    m_cr3 = phys_alloc(PAGE_SIZE);
    CHECK(m_cr3 != NULL);
    memset(m_cr3, 0, PAGE_SIZE);

    //
    // we are going to just assume the file is fine, that is because it should
    // be signed anyways, so if we got so far it should be fine (and TOCTOU is
    // hard)
    //
    LOG_INFO("memory: Kernel mappings");
    CHECK(g_limine_kernel_file_request.response != NULL);
    void* elf_base = g_limine_kernel_file_request.response->kernel_file->address;
    Elf64_Ehdr* ehdr = elf_base;
    Elf64_Phdr* phdrs = elf_base + ehdr->e_phoff;
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type != PT_LOAD) {
            continue;
        }

        uintptr_t vaddr = phdrs[i].p_vaddr;
        uintptr_t vend = phdrs[i].p_vaddr + phdrs[i].p_memsz;

        uintptr_t paddr = (vaddr - m_kernel_virtual_base) + m_kernel_physical_base;
        uintptr_t pend = (vend - m_kernel_virtual_base) + m_kernel_physical_base;

        LOG_INFO("memory: %p-%p (%p-%p) [%c%c%c] %08x",
                 vaddr, vend,
                 paddr, pend,
                 ((phdrs[i].p_flags & PF_R) == 0) ? '-' : 'r',
                 ((phdrs[i].p_flags & PF_W) == 0) ? '-' : 'w',
                 ((phdrs[i].p_flags & PF_X) == 0) ? '-' : 'x',
                 phdrs[i].p_memsz);

        // get the correct flags
        map_flags_t flags = 0;
        if (((phdrs[i].p_flags & PF_W) != 0)) {
            flags |= MAP_PERM_W;
        }
        if (((phdrs[i].p_flags & PF_X) != 0)) {
            CHECK((flags & MAP_PERM_W) == 0);
            flags |= MAP_PERM_X;
        }

        // map it all
        size_t page_num = DIV_ROUND_UP(pend - paddr, SIZE_4KB);
        RETHROW(virt_map_range(paddr, vaddr, page_num, flags));
    }

    // initialize the first 16TB range with top level addressing, this is used later
    // to create RO shadows used by the GC while we are tracing the heap
    for (int i = 0; i < SIZE_16TB / SIZE_512GB; i++) {
        uintptr_t virt = DIRECT_MAP_OFFSET + i * SIZE_512GB;

        page_entry_t* pml4 = &m_cr3[PML4_INDEX(virt)];
        page_entry_t* shadow_pml4 = &m_cr3[PML4_INDEX(virt + SIZE_16TB)];

        // allocate the pml4 if needed
        if (!pml4->present) {
            void* page = phys_alloc(PAGE_SIZE);
            CHECK_ERROR(page != NULL, ERROR_OUT_OF_MEMORY);
            memset(page, 0, SIZE_4KB);

            // this whole area is non-executable, so mark it at the top as such
            pml4->present = 1;
            pml4->writeable = 1;
            pml4->no_execute = 1;
            pml4->frame = DIRECT_TO_PHYS(page) >> 12;
        }

        // copy the shadow
        *shadow_pml4 = *pml4;

        // the GC heap area is also marked as non-writable in the shadow, this is used
        // as a GC barrier while the GC is running in parallel to mutators
        if (0xFFFF810000000000 <= virt && virt < 0xFFFF8E8000000000) {
            shadow_pml4->writeable = 0;
        }
    }

cleanup:
    return err;
}

void switch_page_table() {
    // switch to the page table
    __writecr3(DIRECT_TO_PHYS(m_cr3));

    // enable write protection
    __writecr0(__readcr0() | CR0_WP);
}

bool virt_handle_page_fault(uintptr_t addr) {
    err_t err = NO_ERROR;

    if (
        (0xFFFFA00000000000 <= addr && addr < 0xFFFFA08000000000) ||
        (0xFFFF810000000000 <= addr && addr < 0xFFFF8E8000000000)
    ) {
        // thread structs and gc heap are allocated lazily as required

    } else if (0xFFFF8F0000000000 <= addr && addr < 0xFFFF8F8000000000) {
        // stacks are allocated lazily as required, but we must not allocate if they
        // are inside of the guard zone of the range, which is the bottom 2mb of the
        // stack
        CHECK(ALIGN_DOWN(addr, SIZE_8MB) + SIZE_2MB <= addr);

    } else {
        // unknown area, just return false
        return false;
    }

    // allocate and map the range
    void* page = phys_alloc(PAGE_SIZE);
    CHECK_ERROR(page != NULL, ERROR_OUT_OF_MEMORY);
    memset(page, 0, PAGE_SIZE);

    RETHROW(virt_map_page(DIRECT_TO_PHYS(page), addr & ~PAGE_MASK, MAP_PERM_W));

cleanup:
    return IS_ERROR(err) ? false : true;
}
