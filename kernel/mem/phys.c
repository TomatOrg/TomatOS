#include "phys.h"

#include "early.h"
#include "mem.h"
#include "vmm.h"
#include "thread/cpu_local.h"

#include <sync/irq_spinlock.h>
#include <util/string.h>
#include <util/stb_ds.h>

#include <stdalign.h>

#ifndef NDEBUG
#define BUDDY_ALLOC_SAFETY
#endif

// stubs needed for the library
#define assert ASSERT
static void fflush(void* file) {}
typedef int64_t ssize_t;

// increase alignment to a block size, should be more efficient as we only really
// care for allocating at page intervals in the pmm
#define BUDDY_ALLOC_ALIGN (PAGE_SIZE)

// include the implementation
#define BUDDY_ALLOC_IMPLEMENTATION
#include <buddy_alloc/buddy_alloc.h>
#undef BUDDY_ALLOC_IMPLEMENTATION

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Initialization
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * The buddy allocator instance
 */
static struct buddy* m_buddy = NULL;

/**
 * The spinlock to protect ourselves
 */
static irq_spinlock_t m_palloc_lock = INIT_IRQ_SPINLOCK();

/**
 * Mark all unusable entries
 */
static err_t mark_unusable_ranges() {
    err_t err = NO_ERROR;

    uintptr_t last_usable_end = 0;
    for (int i = 0; i < g_limine_memmap.response->entry_count; i++) {
        struct limine_memmap_entry* entry = g_limine_memmap.response->entries[i];
        if (entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE || entry->type == LIMINE_MEMMAP_USABLE) {
            // mark non-consecutive entries
            buddy_reserve_range(m_buddy, PHYS_TO_DIRECT(last_usable_end), entry->base - last_usable_end);

            // update the last usable range
            last_usable_end = entry->base + entry->length;
        }
    }

    // because we set the top address of the buddy to be the highest usable address
    // we know there is nothing to mark at the end of the buddy itself, so we can just
    // continue normally

cleanup:
    return err;
}

/**
 * Mark bootloader reclaim ranges, we mark them in a different step than the
 * mark unusable so we can reclaim the memory later on
 */
static err_t mark_bootloader_reclaim() {
    err_t err = NO_ERROR;

    for (int i = 0; i < g_limine_memmap.response->entry_count; i++) {
        struct limine_memmap_entry* entry = g_limine_memmap.response->entries[i];
        if (entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
            buddy_reserve_range(m_buddy, PHYS_TO_DIRECT(entry->base), entry->length);
        }
    }

cleanup:
    return err;
}

err_t init_palloc() {
    err_t err = NO_ERROR;

    // find top address and align it to the buddy alignment,
    // we are only going to consider usable and reclaimable
    // addresses of course
    uintptr_t top_address = 0;
    for (int i = 0; i < g_limine_memmap.response->entry_count; i++) {
        uintptr_t base = g_limine_memmap.response->entries[i]->base;
        size_t length = g_limine_memmap.response->entries[i]->length;
        uint64_t type = g_limine_memmap.response->entries[i]->type;
        if (type == LIMINE_MEMMAP_USABLE || type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
            if (base + length > top_address) {
                top_address = base + length;
            }
        }
    }

    // setup the global params for the buddy allocator
    size_t size = ALIGN_UP(buddy_sizeof(top_address), PAGE_SIZE);
    CHECK(size != 0);

    // validate the parameters
    CHECK(top_address <= DIRECT_MAP_SIZE);
    CHECK(size <= BUDDY_TREE_SIZE);

    // map the whole buddy map, allocate it right now since we don't wanna mess too much with demand paging...
    TRACE("Reserving %S for buddy tree", size);
    CHECK_AND_RETHROW(vmm_alloc((void*)BUDDY_TREE_START, size / PAGE_SIZE, MAP_WRITE | MAP_UNMAP_DIRECT));
    memset((void*)BUDDY_TREE_START, 0, size);

    // actually init the buddy
    TRACE("Initializing buddy");
    m_buddy = buddy_init((void*)BUDDY_TREE_START, (void*)DIRECT_MAP_START, top_address);
    CHECK(m_buddy != NULL);

    CHECK_AND_RETHROW(mark_unusable_ranges());
    CHECK_AND_RETHROW(mark_bootloader_reclaim());

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
        buddy_unsafe_release_range(m_buddy, PHYS_TO_DIRECT(entry->base), entry->length);
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
static void* irq_alloc(size_t size) {
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
static void fill_atomic_alloc() {
    while (m_reserved_count < ARRAY_LEN(m_reserved_pages)) {
        void* page = buddy_malloc(m_buddy, PAGE_SIZE);
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

void* palloc(size_t size) {
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
    ptr = buddy_malloc(m_buddy, size);

    // try to fill with pages from the buddy
    fill_atomic_alloc();

    // unlock it
    m_lock_cpu = -1;
    irq_spinlock_unlock(&m_palloc_lock);

    // memset to zero
    if (ptr != NULL) {
        memset(ptr, 0, size);
    }

    return ptr;
}

void* early_palloc(size_t size) {
    irq_spinlock_lock(&m_palloc_lock);
    void* ptr = buddy_malloc(m_buddy, size);
    irq_spinlock_unlock(&m_palloc_lock);
    if (ptr != NULL) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void pfree(void* base) {
    // handle base == NULL
    if (base == NULL) {
        return;
    }

    irq_spinlock_lock(&m_palloc_lock);
    m_lock_cpu = get_cpu_id();

    buddy_free(m_buddy, base);

    m_lock_cpu = -1;
    irq_spinlock_unlock(&m_palloc_lock);
}
