#include "gc.h"

#include <lib/list.h>
#include <sync/spinlock.h>
#include <thread/pcpu.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CPU Local caches
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * The top and bottom address of the GC region of the given order
 */
#define GC_REGION_BOTTOM(order)     ((order * SIZE_512GB) + 0xFFFF810000000000ULL)
#define GC_REGION_TOP(order)        (((order + 1) * SIZE_512GB) + 0xFFFF810000000000ULL)

typedef struct gc_region {
    // lock to protect the order
    // TODO: replace with mutex
    spinlock_t lock;

    // already allocated blocks that can be used
    list_t freelist;

    // the watermark in the region
    void* watermark;

    // the max objects we can fit in this allocator level
    void* top;
} gc_region_t;

static gc_region_t m_gc_global_regions[27];

void gc_init() {
    for (int i = 0; i < ARRAY_LENGTH(m_gc_global_regions); i++) {
        gc_region_t* order = &m_gc_global_regions[i];
        list_init(&order->freelist);
        order->lock = INIT_SPINLOCK();
        order->watermark = (void*)GC_REGION_BOTTOM(i);
        order->top = (void*)GC_REGION_TOP(i);
    }
}

void* tdn_host_gc_alloc(size_t size) {
    // get the region for the correct order
    size_t aligned_size = 1 << ((sizeof(size) * 8) - __builtin_clzl(size - 1));;
    size_t order = ((sizeof(aligned_size) * 8) - 1 - __builtin_clzl(aligned_size)) - 5;
    if (order >= ARRAY_LENGTH(m_gc_global_regions)) {
        return NULL;
    }
    gc_region_t* region = &m_gc_global_regions[order];

    // pop a block from the region
    spinlock_lock(&region->lock);
    void* block = list_pop(&region->freelist);
    if (block == NULL && region->watermark < region->top) {
        block = region->watermark;
        region->watermark += aligned_size;
    }
    spinlock_unlock(&region->lock);

    // if we did not allocate anything, request a GC
    // and try again
    if (block == NULL) {
        // TODO: request an allocation and try again
    }

    // return whatever we allocated
    return block;
}
