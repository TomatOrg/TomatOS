#include <kernel.h>
#include <util/except.h>
#include <util/string.h>
#include "early.h"
#include "mem.h"

static struct stivale2_struct_tag_memmap* m_early_alloc_mmap = NULL;

uintptr_t early_alloc_page_phys() {
    // get the mmap tag
    if (m_early_alloc_mmap == NULL) {
        m_early_alloc_mmap = get_stivale2_tag(STIVALE2_STRUCT_TAG_MEMMAP_ID);
        ASSERT(m_early_alloc_mmap != NULL);
    }

    // find an area large enough and allocate from it
    for (int i = 0; i < m_early_alloc_mmap->entries; i++) {
        struct stivale2_mmap_entry* entry = &m_early_alloc_mmap->memmap[i];

        if (entry->type == STIVALE2_MMAP_USABLE) {
            if (entry->length >= PAGE_SIZE) {
                uintptr_t ptr = entry->base;
                entry->base += PAGE_SIZE;
                entry->length -= PAGE_SIZE;
                return ptr;
            }
        }
    }

    // we could not find enough memory
    ASSERT(!"Failed to allocate early memory");
}
