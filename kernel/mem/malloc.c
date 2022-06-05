#include "malloc.h"

#include "tlsf.h"
#include "mem.h"

#include <sync/irq_spinlock.h>
#include <util/string.h>

static tlsf_t m_tlsf;

static irq_spinlock_t m_tlsf_lock = INIT_IRQ_SPINLOCK();

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
    irq_spinlock_lock(&m_tlsf_lock);
    tlsf_check(m_tlsf);
    irq_spinlock_unlock(&m_tlsf_lock);
}

void* malloc(size_t size) {
    irq_spinlock_lock(&m_tlsf_lock);
    void* ptr = tlsf_memalign(m_tlsf, 16, size);
    irq_spinlock_unlock(&m_tlsf_lock);
    if (ptr != NULL) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void* malloc_aligned(size_t size, size_t alignment) {
    irq_spinlock_lock(&m_tlsf_lock);
    void* ptr = tlsf_memalign(m_tlsf, alignment, size);
    irq_spinlock_unlock(&m_tlsf_lock);
    if (ptr != NULL) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void* realloc(void* ptr, size_t size) {
    irq_spinlock_lock(&m_tlsf_lock);
    ptr = tlsf_realloc(m_tlsf, ptr, size);
    irq_spinlock_unlock(&m_tlsf_lock);
    return ptr;
}

void free(void* ptr) {
    irq_spinlock_lock(&m_tlsf_lock);
    tlsf_free(m_tlsf, ptr);
    irq_spinlock_unlock(&m_tlsf_lock);
}
