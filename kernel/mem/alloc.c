#include "alloc.h"

#include <lib/list.h>
#include <lib/string.h>
#include <sync/spinlock.h>

#include "memory.h"
#include "phys.h"

/**
 * The top and bottom address of the GC region of the given order
 */
#define ALLOC_REGION_BOTTOM(order)     ((order * SIZE_512GB) + 0xFFFFC00000000000ULL)
#define ALLOC_REGION_TOP(order)        (((order + 1) * SIZE_512GB) + 0xFFFFC00000000000ULL)

typedef struct alloc_region {
    // lock to protect the order
    // TODO: replace with mutex
    spinlock_t lock;

    // already allocated blocks that can be used
    void** freelist;

    // the watermark in the region
    void* watermark;

    // the max objects we can fit in this allocator level
    void* top;
} alloc_region_t;

/**
 * The allocator regions
 * goes from 8 bytes to 128mb
 */
static alloc_region_t m_mem_global_regions[24];

void init_alloc(void) {
    for (int i = 0; i < ARRAY_LENGTH(m_mem_global_regions); i++) {
        alloc_region_t* order = &m_mem_global_regions[i];
        order->freelist = NULL;
        order->lock = INIT_SPINLOCK();
        order->watermark = (void*)ALLOC_REGION_BOTTOM(i);
        order->top = (void*)ALLOC_REGION_TOP(i);
    }
}

void* mem_alloc(size_t size) {
    // align to the smallest size we can allocate
    if (size < 8) {
        size = 8;
    }

    // get the region for the correct order
    size_t aligned_size = 1 << ((sizeof(size) * 8) - __builtin_clzl(size - 1));
    int order = ((sizeof(aligned_size) * 8) - 1 - __builtin_clzl(aligned_size)) - 3;
    if (order < 0) {
        order = 0;
    } else if (order >= ARRAY_LENGTH(m_mem_global_regions)) {
        LOG_WARN("Failed to allocate an object of size %lu", size);
        return NULL;
    }

    // TODO: per-cpu cache

    alloc_region_t* region = &m_mem_global_regions[order];

    // pop a block from the region
    spinlock_lock(&region->lock);
    void** block = region->freelist;
    if (block != NULL) {
        region->freelist = *block;
    } else if (region->watermark < region->top) {
        block = region->watermark;
        region->watermark += aligned_size;
    }
    spinlock_unlock(&region->lock);

    return block;
}

void* mem_realloc(void* ptr, size_t size) {
    if (size == 0) {
        mem_free(ptr);
        return NULL;
    }

    // if we have a non-null pointer we can attempt and reuse it
    size_t old_max_size = 0;
    if (ptr != NULL) {
        // figure the current order
        int order = ((uintptr_t)ptr - 0xFFFFC00000000000ULL) / SIZE_512GB;
        old_max_size = 1ull << (order + 3);

        // the size still fits within the current block,
        // return as is
        if (size <= old_max_size) {
            return ptr;
        }
    }

    // allocate the new block
    void* new_ptr = mem_alloc(size);
    if (new_ptr == NULL) {
        return NULL;
    }

    // if we have a previous data then copy and
    // free it
    if (ptr != NULL) {
        memcpy(new_ptr, ptr, old_max_size);
        mem_free(ptr);
    }

    // and return the new data
    return new_ptr;
}

void mem_free(void* ptr) {
    if (ptr == NULL) {
        return;
    }

    int order = ((uintptr_t)ptr - 0xFFFFC00000000000ULL) / SIZE_512GB;
    alloc_region_t* region = &m_mem_global_regions[order];

    // push a the pointer back
    spinlock_lock(&region->lock);
    void** block = ptr;
    *block = region->freelist;
    region->freelist = block;
    spinlock_unlock(&region->lock);
}
