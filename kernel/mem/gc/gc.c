#include "gc.h"

#include <lib/list.h>
#include <sync/spinlock.h>
#include <thread/pcpu.h>

#include "tomatodotnet/types/basic.h"

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
    void** freelist;

    // the watermark in the region
    void* watermark;

    // the max objects we can fit in this allocator level
    void* top;
} gc_region_t;

static gc_region_t m_gc_global_regions[27];

void gc_init() {
    for (int i = 0; i < ARRAY_LENGTH(m_gc_global_regions); i++) {
        gc_region_t* order = &m_gc_global_regions[i];
        order->freelist = NULL;
        order->lock = SPINLOCK_INIT;
        order->watermark = (void*)GC_REGION_BOTTOM(i);
        order->top = (void*)GC_REGION_TOP(i);
    }
}

Object tdn_host_gc_alloc(ObjectVTable* vtable, size_t size, size_t alignment) {
    // align to the smallest size we can allocate
    if (size < 32) {
        size = 32;
    }

    // get the region for the correct order
    size_t aligned_size = 1 << ((sizeof(size) * 8) - __builtin_clzl(size - 1));
    int order = ((sizeof(aligned_size) * 8) - 1 - __builtin_clzl(aligned_size)) - 5;
    if (order < 0) {
        order = 0;
    } else if (order >= ARRAY_LENGTH(m_gc_global_regions)) {
        WARN("Failed to allocate an object of size %lu", size);
        return NULL;
    }

    // TODO: per-cpu cache

    gc_region_t* region = &m_gc_global_regions[order];

    // pop a block from the region
    spinlock_acquire(&region->lock);
    void** block = region->freelist;
    if (block != NULL) {
        region->freelist = *block;
    } else if (region->watermark < region->top) {
        block = region->watermark;
        region->watermark += aligned_size;
    }
    spinlock_release(&region->lock);

    // if we did not allocate anything, request a GC
    // and try again
    if (block == NULL) {
        // TODO: request a gc and try again
    }

    // return whatever we allocated
    Object obj = (Object)block;
    obj->VTable = vtable;
    return obj;
}

void tdn_host_gc_register_root(void* root) {
    // TODO: register a new root for the collector
}

void tdn_host_gc_pin_object(void* object) {
    // TODO: pin an object so it won't get collected
}