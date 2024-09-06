#include "alloc.h"

#include <lib/list.h>
#include <lib/string.h>
#include <sync/spinlock.h>

#include "memory.h"
#include "phys.h"

// min allocation size is 64 bytes
#define MIN_POOL_SHIFT  6
#define MIN_POOL_SIZE   (1 << MIN_POOL_SHIFT)

// max allocation size is 2k
#define MAX_POOL_SHIFT  (PAGE_SHIFT - 1)
#define MAX_POOL_SIZE   (1 << MAX_POOL_SHIFT)

// the pool entries
#define MAX_POOL_INDEX  (MAX_POOL_SHIFT - MIN_POOL_SHIFT + 1)

typedef struct pool_header {
    size_t size;
} pool_header_t;

typedef struct free_pool_header {
    pool_header_t header;
    list_entry_t link;
} free_pool_header_t;

/**
 * Global lock to protect the allocator
 */
static spinlock_t m_alloc_lock = INIT_SPINLOCK();

static list_entry_t m_alloc_pool_lists[MAX_POOL_INDEX] = {
    LIST_INIT(&m_alloc_pool_lists[0]),
    LIST_INIT(&m_alloc_pool_lists[1]),
    LIST_INIT(&m_alloc_pool_lists[2]),
    LIST_INIT(&m_alloc_pool_lists[3]),
    LIST_INIT(&m_alloc_pool_lists[4]),
    LIST_INIT(&m_alloc_pool_lists[5]),
};
STATIC_ASSERT(ARRAY_LENGTH(m_alloc_pool_lists) == 6);

static free_pool_header_t* alloc_pool_by_index(size_t pool_index) {
    free_pool_header_t* hdr = NULL;

    // attempt to allocate from the given pool size
    if (pool_index == MAX_POOL_INDEX) {
        // we reached the max pool size, use the page allocator
        // directly for this case
        hdr = phys_alloc(MAX_POOL_SIZE << 1);

    } else if (!list_is_empty(&m_alloc_pool_lists[pool_index])) {
        // we have an empty entry, use it
        hdr = containerof(m_alloc_pool_lists[pool_index].next, free_pool_header_t, link);
        list_del(&hdr->link);

    } else {
        // attempt to allocate from the next level
        hdr = alloc_pool_by_index(pool_index + 1);
        if (hdr != NULL) {
            // split the allocated entry into two entries, one we are going
            // to add to our pool, and one is going to be for returning to
            // the caller
            hdr->header.size >>= 1;
            list_add(&m_alloc_pool_lists[pool_index], &hdr->link);

            hdr = (free_pool_header_t*)((uintptr_t)hdr + hdr->header.size);
        }
    }

    // set the header for this entry
    if (hdr != NULL) {
        hdr->header.size = MIN_POOL_SIZE << pool_index;
    }

    return hdr;
}

static size_t highest_set_bit(uint32_t val) {
    return 31 - __builtin_clz(val);
}

static void free_pool_by_index(free_pool_header_t* free_pool_header) {
    const size_t pool_index = highest_set_bit(free_pool_header->header.size) - MIN_POOL_SHIFT;
    list_add(&m_alloc_pool_lists[pool_index], &free_pool_header->link);
}

void* mem_alloc(size_t size) {
    // adjust for allocation header
    size_t wanted_size = size;
    size += sizeof(pool_header_t);

    void* ptr = NULL;

    if (size > MAX_POOL_SIZE) {
        // too large, use the page allocator directly
        size = ALIGN_UP(size, PAGE_SIZE);
        pool_header_t* header = phys_alloc(size);
        if (header != NULL) {
            header->size = size;
            ptr = header + 1;
        }
    } else {
        // calculate the pool size properly
        size = (size + MIN_POOL_SIZE - 1) >> MIN_POOL_SHIFT;
        size_t pool_index = highest_set_bit(size);
        if ((size & (size - 1)) != 0) {
            pool_index++;
        }


        // take the lock
        bool save = irq_save();
        spinlock_lock(&m_alloc_lock);

        // and allocate it
        pool_header_t* header = (pool_header_t*)alloc_pool_by_index(pool_index);
        if (header != NULL) {
            ptr = header + 1;
        }

        spinlock_unlock(&m_alloc_lock);
        irq_restore(save);
    }

    if (ptr != NULL) {
        memset(ptr, 0, wanted_size);
    }

    return ptr;
}

static inline size_t mem_get_alloc_size(void* ptr) {
    pool_header_t* header = ((pool_header_t*)ptr - 1);
    return header->size - sizeof(pool_header_t);

}

void* mem_realloc(void* ptr, size_t size) {
    if (ptr == NULL) {
        return mem_alloc(size);
    }

    // TODO: maybe free upper part if possible
    size_t old_size = mem_get_alloc_size(ptr);
    if (old_size >= size) {
        return ptr;
    }

    // allocate new range
    void* new_ptr = mem_alloc(size);
    if (new_ptr == NULL) {
        return NULL;
    }

    // copy it
    memcpy(new_ptr, ptr, old_size);
    memset(new_ptr + old_size, 0, size - old_size);

    // free the old range
    mem_free(ptr);

    return new_ptr;
}

void mem_free(void* ptr) {
    // ignore null free
    if (ptr == NULL) {
        return;
    }

    // get the real header
    free_pool_header_t* header = (free_pool_header_t*)((pool_header_t*)ptr - 1);

    if (header->header.size > MAX_POOL_SIZE) {
        // this is from the page allocator
        phys_free(header);

    } else {
        // this is from the pool, take the lock
        // and free it
        bool save = irq_save();
        spinlock_lock(&m_alloc_lock);
        free_pool_by_index(header);
        spinlock_unlock(&m_alloc_lock);
        irq_restore(save);
    }
}
