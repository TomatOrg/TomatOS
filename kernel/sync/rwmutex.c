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

#include "rwmutex.h"

#include <stdatomic.h>

#define RWMUTEX_MAX_READERS (1 << 30)

// unlike fetch add, this does add fetch
#define atomic_add(ptr, value) \
    ({ \
        int32_t __value = value; \
        atomic_fetch_add(ptr, __value) + __value; \
    })

void rwmutex_rlock(rwmutex_t* rw) {
    if (atomic_add(&rw->reader_count, 1) < 0) {
        // A writer is pending, wait for it.
        semaphore_acquire(&rw->reader_sem, false, -1);
    }
}

static void rwlock_unlock_slow(rwmutex_t* rw, int32_t r) {
    // make sure the mutex is not locked
    ASSERT(r + 1 != 0 && r + 1 != -RWMUTEX_MAX_READERS);

    // A writer is pending.
    if (atomic_add(&rw->reader_wait, -1) == 0) {
        // The last reader unblocks the writer.
        semaphore_release(&rw->writer_sem, false);
    }
}

void rwmutex_runlock(rwmutex_t* rw) {
    int32_t r = atomic_add(&rw->reader_count, -1);
    if (r < 0) {
        // Outlined slow-path to allow the fast-path to be inlined
        rwlock_unlock_slow(rw, r);
    }
}

void rwmutex_lock(rwmutex_t* rw) {
    // First, resolve competition with other writers.
    mutex_lock(&rw->mutex);

    // Announce to readers there is a pending writer
    int32_t r = atomic_add(&rw->reader_count, -RWMUTEX_MAX_READERS) + RWMUTEX_MAX_READERS;

    // Wait for active readers.
    if (r != 0 && atomic_add(&rw->reader_wait, r) != 0) {
        semaphore_acquire(&rw->writer_sem, false, -1);
    }
}

void rwmutex_unlock(rwmutex_t* rw) {
    // Announce to readers there is no active writer.
    int32_t r = atomic_add(&rw->reader_count, RWMUTEX_MAX_READERS);
    ASSERT(r < RWMUTEX_MAX_READERS);

    // Unblock blocked readers, if any.
    for (int i = 0; i < r; i++) {
        semaphore_release(&rw->reader_sem, false);
    }

    // Allow other writers to proceed.
    mutex_unlock(&rw->mutex);
}
