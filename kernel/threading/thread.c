/*
 * Code taken and modified from Go
 *
 * Copyright (c) 2009 The Go Authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *    * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "thread.h"

#include "cpu_local.h"

#include <arch/intrin.h>
#include <util/except.h>

#include "scheduler.h"

#include <stdatomic.h>


//
// Global waiting thread cache
//
static spinlock_t m_global_wt_lock;
static waiting_thread_t* m_global_wt_cache;

//
// Per-cpu waiting thread cache
//
static waiting_thread_t* CPU_LOCAL m_wt_cache[128];
static uint8_t CPU_LOCAL m_wt_cache_len = 0;

waiting_thread_t* acquire_waiting_thread() {
    // we disable interrupts in here so we can do stuff
    // atomically on the current core
    bool ints = (__readeflags() & BIT9) ? true : false;
    _disable();

    // check if we have any local cached
    if (m_wt_cache_len == 0) {

        // try to get a bunch from the central cache, maximum to half the size
        // of the local cache
        spinlock_lock(&m_global_wt_lock);
        while ((m_wt_cache_len < ARRAY_LEN(m_wt_cache) / 2) && m_global_wt_cache != NULL) {
            waiting_thread_t* wt = m_global_wt_cache;
            m_global_wt_cache = wt->next;
            wt->next = NULL;
            m_wt_cache[m_wt_cache_len++] = wt;
        }
        spinlock_unlock(&m_global_wt_lock);

        if (m_wt_cache_len == 0) {
            // central cache is empty, allocate a new one
            m_wt_cache[m_wt_cache_len++] = malloc(sizeof(waiting_thread_t));
        }
    }

    // pop one
    waiting_thread_t* wt = m_wt_cache[--m_wt_cache_len];
    m_wt_cache[m_wt_cache_len] = NULL;

    if (ints) {
        _enable();
    }

    return wt;
}

void release_waiting_thread(waiting_thread_t* wt) {
    // we disable interrupts in here so we can do stuff
    // atomically on the current core
    bool ints = (__readeflags() & BIT9) ? true : false;
    _disable();

    if (m_wt_cache_len == ARRAY_LEN(m_wt_cache)) {
        // Transfer half of the local cache to the central cache
        waiting_thread_t* first = NULL;
        waiting_thread_t* last = NULL;

        while (m_wt_cache_len > ARRAY_LEN(m_wt_cache) / 2) {
            waiting_thread_t* p = m_wt_cache[--m_wt_cache_len];
            m_wt_cache[m_wt_cache_len] = NULL;
            if (first == NULL) {
                first = p;
            } else {
                last->next = p;
            }
            last = p;
        }

        spinlock_lock(&m_global_wt_lock);
        last->next = m_global_wt_cache;
        m_global_wt_cache = last;
        spinlock_unlock(&m_global_wt_lock);
    }

    // put into the local cache
    m_wt_cache[m_wt_cache_len++] = wt;

    if (ints) {
        _enable();
    }
}

thread_status_t get_thread_status(thread_t* thread) {
    return atomic_load(&thread->status);
}

void cas_thread_state(thread_t* thread, thread_status_t old, thread_status_t new) {
    thread_status_t old_value = old;
    for (int i = 0; !atomic_compare_exchange_weak(&thread->status, &old_value, new); i++, old_value = old) {
        if (old == THREAD_STATUS_WAITING && thread->status == THREAD_STATUS_RUNNABLE) {
            ASSERT(!"Waiting for THREAD_STATUS_WAITING but is THREAD_STATUS_RUNNABLE");
        }

        // TODO: go does a yield in here but it is because of garbage collection, we don't really
        //       care about this I think
    }
}
