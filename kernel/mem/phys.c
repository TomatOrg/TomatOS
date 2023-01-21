#include "phys.h"

#include "early.h"
#include "mem.h"
#include "vmm.h"
#include "thread/cpu_local.h"
#include "arch/idt.h"

#include <sync/irq_spinlock.h>
#include <util/string.h>
#include <util/stb_ds.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Buddy allocator
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Buddy levels
 * 0  - 4kb
 * 1  - 8kb
 * 2  - 16kb
 * 3  - 32kb
 * 4  - 64kb
 * 5  - 128kb
 * 6  - 256kb
 * 7  - 512kb
 * 8  - 1MB
 * 9  - 2MB
 */
static list_t m_buddy_levels[10];

INTERRUPT static int get_level_by_size(size_t size) {
    // allocation is too big, return invalid
    if (size > SIZE_2MB) {
        return -1;
    }

    // allocation is too small, round to 4kb
    if (size < SIZE_4KB) {
        size = SIZE_4KB;
    }

    // align up to next power of two
    size = 1 << (32 - __builtin_clz(size - 1));

    // and now calculate the log2
    int level = 32 - __builtin_clz(size) - 1;

    // ignore the first 12 levels because they are less than 4kb
    return level - 12;
}

static void init_buddy_levels() {
    for (int i = 0; i < ARRAY_LEN(m_buddy_levels); i++) {
        list_init(&m_buddy_levels[i]);
    }
}

INTERRUPT static void* allocate_from_level(int level) {
    int block_at_level;
    list_entry_t* block = NULL;

    // -1 means that we wanted to allocate too much memory
    if (level == -1) {
        return NULL;
    }

    ASSERT(level < ARRAY_LEN(m_buddy_levels));

    // search from the level a block to allocate, and go
    // up if the list is empty
    for (block_at_level = level; block_at_level < ARRAY_LEN(m_buddy_levels); block_at_level++) {
        if (!list_is_empty(&m_buddy_levels[block_at_level])) {
            block = list_pop(&m_buddy_levels[block_at_level]);
            break;
        }
    }

    // no more space
    if (block == NULL) {
        return NULL;
    }

    // check if we need to split
    if (block_at_level != level) {
        // split the block until we reach
        // the requested level
        while (block_at_level > level) {
            // calculate the size
            size_t block_size = 1 << (block_at_level + 12);

            // split it to two, adding the higher half to the level below us
            list_entry_t* upper = (list_entry_t*)(((uintptr_t)block) + block_size / 2);
            list_add(&m_buddy_levels[block_at_level - 1], upper);

            // mark the upper block as the new level it is at
            PAGE_TABLE_PML1[PML1_INDEX(upper)].buddy_level = (block_at_level - 1) + 1;
            PAGE_TABLE_PML1[PML1_INDEX(upper)].buddy_alloc = false;

            // decrease the level we are at
            block_at_level--;
        }
    }

    // mark our block as allocated by setting the available to zero
    PAGE_TABLE_PML1[PML1_INDEX(block)].buddy_level = level + 1;
    PAGE_TABLE_PML1[PML1_INDEX(block)].buddy_alloc = true;

    // return the block we found
    return block;
}

INTERRUPT static void free_at_level(void* ptr, int level) {
    ASSERT(level < ARRAY_LEN(m_buddy_levels));

    while (level < (ARRAY_LEN(m_buddy_levels) - 1)) {
        // get the current block size
        size_t block_size = 1 << (level + 12);

        // 1 if the neighbor is above us, -1 if it is below us
        int neighbor = ((uintptr_t)ptr & ((block_size * 2) - 1)) == 0 ? 1 : -1;

        list_entry_t* neighbor_entry = ptr + neighbor * block_size;
        page_entry_4kb_t* neighbor_pt = &PAGE_TABLE_PML1[PML1_INDEX(neighbor_entry)];

        // check if there is a neighbor
        if (neighbor_pt->buddy_level == 0) {
            break;
        }

        // check that neighbor is not allocated
        if (neighbor_pt->buddy_alloc) {
            break;
        }

        // check if the neighbor is at the same level, if it
        // is then merge with it
        int neighbor_level = neighbor_pt->buddy_level - 1;
        if (neighbor_level != level) {
            break;
        }

        // remove from the current level
        list_del(neighbor_entry);

        // get the new pointer, if the neighbor was below then
        // get the neighbor entry
        if (neighbor == -1) {
            ptr -= block_size;
        }

        // next level please
        level++;
    }

    // set the block size now, to indicate this entire range is free for this level
    PAGE_TABLE_PML1[PML1_INDEX(ptr)].buddy_level = level + 1;
    PAGE_TABLE_PML1[PML1_INDEX(ptr)].buddy_alloc = false;

    // we now know the correct level, add the ptr to it
    list_entry_t* ptr_entry = ptr;
    list_add(&m_buddy_levels[level], ptr_entry);
}

static void add_memory(void* base, size_t length) {
    // get this in pages
    length /= SIZE_4KB;

    // treat as direct map
    base = PHYS_TO_DIRECT(base);

    // start by aligning it
    while (((uintptr_t)base % SIZE_2MB) != 0 && length != 0) {
        free_at_level(base, 0);
        base += SIZE_4KB;
        length--;
    }

    // now add 2mb blocks
    while (length > (SIZE_2MB / SIZE_4KB)) {
        free_at_level(base, 9);
        length -= (SIZE_2MB / SIZE_4KB);
        base += SIZE_2MB;
    }

    // and add the rest
    while (length != 0) {
        free_at_level(base, 0);
        base += SIZE_4KB;
        length--;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Initialization
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * The spinlock to protect ourselves
 */
static irq_spinlock_t m_palloc_lock = INIT_IRQ_SPINLOCK();

err_t init_palloc() {
    err_t err = NO_ERROR;

    // make sure the levels are fine
    init_buddy_levels();

    // setting up buddy
    for (int i = 0; i < g_limine_memmap.response->entry_count; i++) {
        uintptr_t base = g_limine_memmap.response->entries[i]->base;
        size_t length = g_limine_memmap.response->entries[i]->length;
        uint64_t type = g_limine_memmap.response->entries[i]->type;
        if (type != LIMINE_MEMMAP_USABLE) {
            continue;
        }

        add_memory((void*)base, length);
    }

cleanup:
    return err;
}

err_t palloc_reclaim() {
    err_t err = NO_ERROR;
    struct limine_memmap_entry* to_reclaim = NULL;

    // gather and copy entries that we need (we must copy them
    // because otherwise we are going to reclaim them)
    for (int i = 0; i < g_limine_memmap.response->entry_count; i++) {
        if (g_limine_memmap.response->entries[i]->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
            arrpush(to_reclaim, *g_limine_memmap.response->entries[i]);
        }
    }

    // now actually reclaim them
    TRACE("Reclaiming memory");
    for (int i = 0; i < arrlen(to_reclaim); i++) {
        struct limine_memmap_entry* entry = &to_reclaim[i];
        TRACE("\t%p-%p: %S", entry->base, entry->base + entry->length, entry->length);
        add_memory((void*)entry->base, entry->length);
    }

cleanup:
    arrfree(to_reclaim);
    return err;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IRQ allocation
//
// This is meant for cases where a palloc caused a PF for demand paging (can happen with stack growing)
// which means that we need to properly be able to allocate memory for such a case without trying to take
// the allocator lock again
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * The lock which taken the cpu
 */
static int m_lock_cpu = -1;

/**
 * The reserved pages
 */
static void* m_reserved_pages[16];

/**
 * The amount of reserved pages
 */
static size_t m_reserved_count = 0;

/**
 * Used for allocation of memory inside of IRQ context while the palloc lock is already taken
 * only one cpu can be the owner of the lock so only one cpu can use this reserved memory pool.
 * All cpus can fill the pool tho as long as the hold the lock itself.
 *
 * Only up to 4k allocation size is supported by this method.
 */
INTERRUPT static void* irq_alloc(size_t size) {
    if (m_lock_cpu == get_cpu_id()) {
        ASSERT(size <= PAGE_SIZE);
        ASSERT(m_reserved_count > 0);
        return m_reserved_pages[--m_reserved_count];
    }

    return NULL;
}

/**
 * Fill the reserved pool with pages so IRQ context can use them
 */
static void fill_irq_alloc() {
    while (m_reserved_count < ARRAY_LEN(m_reserved_pages)) {
        void* page = allocate_from_level(0);
        if (page == NULL) {
            WARN("phys: out of memory to fill the reserved pages pool");
            return;
        }
        memset(page, 0, PAGE_SIZE);
        m_reserved_pages[m_reserved_count++] = page;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

INTERRUPT void* palloc(size_t size) {
    // first try to do an irq allocation, this will only
    // work if the current CPU already locked the palloc
    // lock and then we got into this path again (see more
    // in the function desc)
    void* ptr = irq_alloc(size);
    if (ptr != NULL) {
        return ptr;
    }

    // take the lock of the pmm, and set that we took the lock
    irq_spinlock_lock(&m_palloc_lock);
    m_lock_cpu = get_cpu_id();

    // allocate from the buddy
    ptr = allocate_from_level(get_level_by_size(size));

    // try to fill with pages from the buddy
    fill_irq_alloc();

    // unlock it
    m_lock_cpu = -1;
    irq_spinlock_unlock(&m_palloc_lock);

    // memset to zero
    if (ptr != NULL) {
        memset(ptr, 0, size);
    } else {
        ASSERT(!"Out of memory");
    }

    return ptr;
}

void* early_palloc(size_t size) {
    irq_spinlock_lock(&m_palloc_lock);
    void* ptr = allocate_from_level(get_level_by_size(size));
    irq_spinlock_unlock(&m_palloc_lock);
    if (ptr != NULL) {
        memset(ptr, 0, size);
    }
    return ptr;
}

INTERRUPT void pfree(void* base) {
    // handle base == NULL
    if (base == NULL) {
        return;
    }

    irq_spinlock_lock(&m_palloc_lock);
    m_lock_cpu = get_cpu_id();

    // make sure this entry is allocated
    ASSERT(PAGE_TABLE_PML1[PML1_INDEX(base)].buddy_alloc);

    // get the level, and make sure it is valid and that
    // the base is aligned to the claimed base
    int level = PAGE_TABLE_PML1[PML1_INDEX(base)].buddy_level;
    ASSERT(level != 0);
    level--;
    ASSERT(((uintptr_t)base & ((1 << (level + 12)) - 1)) == 0);

    // free it
    free_at_level(base, level);

    m_lock_cpu = -1;
    irq_spinlock_unlock(&m_palloc_lock);
}
