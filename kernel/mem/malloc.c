#include "malloc.h"

#include "tlsf.h"
#include "mem.h"

#include <sync/spinlock.h>
#include <util/string.h>

static tlsf_t m_tlsf;

static spinlock_t m_tlsf_lock = INIT_SPINLOCK();

err_t init_malloc() {
    err_t err = NO_ERROR;

    // Allocate memory for the allocator
    void* tlsf_mem = palloc(tlsf_size());
    CHECK(tlsf_mem != NULL);

    // Init the allocator
    m_tlsf = tlsf_create(tlsf_mem);
    CHECK(m_tlsf != NULL);

    // Add the pool itself
    pool_t pool = tlsf_add_pool(m_tlsf, (void*)KERNEL_HEAP_START, KERNEL_HEAP_SIZE);
    CHECK(pool != NULL);

cleanup:
    return err;
}

void check_malloc() {
    spinlock_lock(&m_tlsf_lock);
    tlsf_check(m_tlsf);
    spinlock_unlock(&m_tlsf_lock);
}

void* malloc(size_t size) {
    spinlock_lock(&m_tlsf_lock);
    void* ptr = tlsf_memalign(m_tlsf, 16, size);
    spinlock_unlock(&m_tlsf_lock);
    if (ptr != NULL) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void* malloc_aligned(size_t size, size_t alignment) {
    spinlock_lock(&m_tlsf_lock);
    void* ptr = tlsf_memalign(m_tlsf, alignment, size);
    spinlock_unlock(&m_tlsf_lock);
    if (ptr != NULL) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void* realloc(void* ptr, size_t size) {
    spinlock_lock(&m_tlsf_lock);
    ptr = tlsf_realloc(m_tlsf, ptr, size);
    spinlock_unlock(&m_tlsf_lock);
    return ptr;
}

void free(void* ptr) {
    spinlock_lock(&m_tlsf_lock);
    tlsf_free(m_tlsf, ptr);
    spinlock_unlock(&m_tlsf_lock);
}
