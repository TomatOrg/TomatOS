#include "gc.h"

#include <sync/ticketlock.h>
#include <mem/malloc.h>
#include <proc/proc.h>
#include <mem/phys.h>
#include <util/string.h>

/**
 * This code is based on https://github.com/GregorR/ggggc
 *
 * License (with modifications):
 * Copyright (c) 2014, 2015 Gregor Richards
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GC Pool management
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Pools which are freely available
 * TODO: use mutex instead?
 */
static PROCESS_LOCAL spinlock_t m_free_pools_lock = INIT_SPINLOCK();
static PROCESS_LOCAL gc_pool_t* m_free_pools_head = NULL;
static PROCESS_LOCAL gc_pool_t* m_free_pools_tail = NULL;

static void* gc_alloc_pool(bool must_succeed) {
    // we can assume it is properly aligned
    void* ptr = palloc(GC_POOL_BYTES);
    ASSERT((uintptr_t)ptr % GC_POOL_BYTES);
    if (ptr == NULL) {
        ASSERT(!must_succeed);
        return NULL;
    }
    return ptr;
}

static gc_pool_t* gc_new_pool(bool must_succeed) {
    gc_pool_t* ret = NULL;

    // try to reuse a pool
    if (m_free_pools_head != NULL) {
        spinlock_lock(&m_free_pools_lock);
        if (m_free_pools_head != NULL) {
            ret = m_free_pools_head;
            m_free_pools_head = m_free_pools_head->next;
            if (m_free_pools_head == NULL) {
                m_free_pools_tail = NULL;
            }
        }
        spinlock_unlock(&m_free_pools_lock);
    }

    // otherwise allocate one
    if (ret == NULL) {
        ret = gc_alloc_pool(must_succeed);
    }

    if (ret == NULL) {
        return ret;
    }

    // set it up
    ret->next = NULL;
    ret->free = ret->start;
    ret->end = (size_t*)((uintptr_t)ret + GC_POOL_BYTES);

    return ret;
}

static gc_pool_t* gc_new_pool_gen(uint8_t gen, bool must_succeed) {
    gc_pool_t* ret = gc_new_pool(must_succeed);
    if (ret == NULL) {
        return NULL;
    }

    ret->gen = gen;

    // clear the remembered set
    if (gen > 0) {
        memset(ret->remember, 0, GC_CARDS_PER_POOL);
    }

    // the first object in the first usable card
    ret->first_object[GC_CARD_OF(ret->start)] = (((uintptr_t)ret->start) & GC_CARD_INNER_MASK) / sizeof(size_t);

    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GC Allocation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Generation 0 pools
 */
static THREAD_LOCAL gc_pool_t* m_gc_gen0 = NULL;

/**
 * The current allocation pool for generation 0
 */
static THREAD_LOCAL gc_pool_t* m_gc_pool0 = NULL;

typedef struct gc_header {
    type_t type;
} gc_header_t;

static void* gc_alloc_raw(type_t type, size_t size) {
    gc_pool_t* pool = NULL;
    gc_header_t* ret = NULL;

    // we want this in words
    size = ALIGN_UP(size, sizeof(size_t)) / sizeof(size_t) + GC_WORD_SIZEOF(gc_header_t);

    do {
        if (m_gc_pool0 != NULL) {
            pool = m_gc_pool0;
        } else {
            pool = gc_new_pool_gen(0, true);
            m_gc_gen0 = pool;
            m_gc_pool0 = pool;
        }

        // do we have enough space?
        if (pool->end - pool->free >= size) {
            // good, allocate here
            ret = (gc_header_t*)pool->free;
            pool->free += size;

            // set it up
            ret->type = type;
            memset(ret + 1, 0, size * sizeof(size_t) - sizeof(gc_header_t));
        } else if (pool->next != NULL) {
            pool = pool->next;
            m_gc_pool0 = pool;
        } else {
            // we need to collect
            ASSERT(!"TODO: collect");
        }
    } while (ret == NULL);

    return ret;
}

void* gc_alloc(type_t type) {
    return gc_alloc_raw(type, GC_WORD_SIZEOF(type->managed_size));
}

void* gc_alloc_array(type_t type, size_t size) {
    return malloc(type->stack_size * size);
}
