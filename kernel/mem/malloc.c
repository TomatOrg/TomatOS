#include <sync/ticketlock.h>
#include "malloc.h"

#include "tlsf.h"
#include "mem.h"

static tlsf_t m_tlsf;

static ticketlock_t m_tlsf_lock = INIT_TICKETLOCK();

err_t init_malloc() {
    err_t err = NO_ERROR;

    m_tlsf = tlsf_create_with_pool((void*)KERNEL_HEAP_START, KERNEL_HEAP_SIZE);
    CHECK(m_tlsf != NULL);

cleanup:
    return err;
}

void* malloc(size_t size) {
    ticketlock_lock(&m_tlsf_lock);
    void* ptr = tlsf_malloc(m_tlsf, size);
    ticketlock_unlock(&m_tlsf_lock);
    return ptr;
}

void* realloc(void* ptr, size_t size) {
    ticketlock_lock(&m_tlsf_lock);
    ptr = tlsf_realloc(m_tlsf, ptr, size);
    ticketlock_unlock(&m_tlsf_lock);
    return ptr;
}

void free(void* ptr) {
    ticketlock_lock(&m_tlsf_lock);
    tlsf_free(m_tlsf, ptr);
    ticketlock_unlock(&m_tlsf_lock);
}
