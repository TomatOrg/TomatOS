#include "malloc.h"

#include "tlsf.h"
#include "mem.h"
#include "sync/mutex.h"

#include <sync/irq_spinlock.h>
#include <util/string.h>

// kernel heap
static tlsf_t* m_tlsf;
static spinlock_t m_tlsf_lock = INIT_SPINLOCK();

// low memory heap
static tlsf_t* m_lowmem_tlsf;
static spinlock_t m_lowmem_tlsf_lock = INIT_SPINLOCK();

err_t init_malloc() {
    err_t err = NO_ERROR;

    {
        // Allocate memory for the allocator
        void* tlsf_mem = palloc(tlsf_size());
        CHECK(tlsf_mem != NULL);

        // Init the allocator
        m_tlsf = tlsf_create(tlsf_mem);
        CHECK(m_tlsf != NULL);

        // Add the pool itself
        tlsf_pool_t* pool = tlsf_add_pool(m_tlsf, (void*)KERNEL_HEAP_START, KERNEL_HEAP_SIZE);
        CHECK(pool != NULL);
    }

    {
        // Allocate memory for the low memory allocator
        void* lowmem_tlsf_mem = palloc(tlsf_size());
        CHECK(lowmem_tlsf_mem != NULL);

        // Init the allocator
        m_lowmem_tlsf = tlsf_create(lowmem_tlsf_mem);
        CHECK(m_lowmem_tlsf != NULL);

        // Add the pool itself
        tlsf_pool_t* lowmem_pool = tlsf_add_pool(m_lowmem_tlsf, (void*)KERNEL_LOW_MEM_HEAP_START, KERNEL_LOW_MEM_HEAP_SIZE);
        CHECK(lowmem_pool != NULL);
    }

cleanup:
    return err;
}

void check_malloc() {
    if (m_tlsf != NULL) {
        spinlock_lock(&m_tlsf_lock);
        tlsf_check(m_tlsf);
        spinlock_unlock(&m_tlsf_lock);
    }
}

void* malloc(size_t size) {
    spinlock_lock(&m_tlsf_lock);

    void* ptr = tlsf_memalign(m_tlsf, 16, size);

#ifndef DNDEBUG
    tlsf_track_allocation(ptr, __builtin_return_address(0));
    tlsf_track_free(ptr, 0);
#endif

    spinlock_unlock(&m_tlsf_lock);

    if (ptr != NULL) {
        memset(ptr, 0, size);
    }

    return ptr;
}

void* malloc_aligned(size_t size, size_t alignment) {
    if (alignment == 0) {
        alignment = 16;
    }

    spinlock_lock(&m_tlsf_lock);

    void* ptr = tlsf_memalign(m_tlsf, alignment, size);

#ifndef DNDEBUG
    tlsf_track_allocation(ptr, __builtin_return_address(0));
    tlsf_track_free(ptr, 0);
#endif

    spinlock_unlock(&m_tlsf_lock);

    if (ptr != NULL) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void* realloc(void* ptr, size_t size) {
    spinlock_lock(&m_tlsf_lock);

    ptr = tlsf_realloc(m_tlsf, ptr, size);

#ifndef DNDEBUG
    if (ptr != NULL) {
        tlsf_track_allocation(ptr, __builtin_return_address(0));
        tlsf_track_free(ptr, 0);
    }
#endif

    spinlock_unlock(&m_tlsf_lock);

    return ptr;
}

void free(void* ptr) {
    spinlock_lock(&m_tlsf_lock);

    if (ptr) tlsf_track_free(ptr, __builtin_return_address(0));
    tlsf_free(m_tlsf, ptr);

    spinlock_unlock(&m_tlsf_lock);
}

void* lowmem_malloc(size_t size) {
    spinlock_lock(&m_lowmem_tlsf_lock);

    void* ptr = tlsf_memalign(m_lowmem_tlsf, 16, size);

#ifndef DNDEBUG
    tlsf_track_allocation(ptr, __builtin_return_address(0));
    tlsf_track_free(ptr, 0);
#endif

    spinlock_unlock(&m_lowmem_tlsf_lock);

    if (ptr != NULL) {
        memset(ptr, 0, size);
    }

    return ptr;
}

void lowmem_free(void* ptr) {
    spinlock_lock(&m_lowmem_tlsf_lock);

    if (ptr) tlsf_track_free(ptr, __builtin_return_address(0));
    tlsf_free(m_lowmem_tlsf, ptr);

    spinlock_unlock(&m_lowmem_tlsf_lock);
}
