#include "early.h"
#include "mem.h"

#include <kernel.h>

#include <util/except.h>
#include <util/string.h>

// TODO: we probably want to save aside which pages we allocated for
//       this for better memory tracking

uintptr_t early_alloc_page_phys() {
    // find an area large enough and allocate from it
    for (int i = 0; i < g_limine_memmap.response->entry_count; i++) {
        struct limine_memmap_entry* entry = g_limine_memmap.response->entries[i];

        if (entry->type == LIMINE_MEMMAP_USABLE) {
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
    return 0;
}
