#include "malloc.h"

#include "tlsf.h"
#include "mem.h"

#include <sync/spinlock.h>
#include <util/string.h>

static tlsf_t m_tlsf;

static spinlock_t m_tlsf_lock = INIT_SPINLOCK();

err_t init_malloc() {
    err_t err = NO_ERROR;

    m_tlsf = tlsf_create_with_pool((void*)KERNEL_HEAP_START, KERNEL_HEAP_SIZE);
    CHECK(m_tlsf != NULL);

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
    void* ptr = tlsf_malloc(m_tlsf, size);
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
