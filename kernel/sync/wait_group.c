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

#include "wait_group.h"

#include <stdatomic.h>

void wait_group_add(wait_group_t* wg, int delta) {
    uint64_t state = atomic_fetch_add(&wg->state, (uint64_t)delta << 32) + ((uint64_t)delta << 32);
    int32_t v = (int32_t)(state >> 32);
    uint32_t w = state;

    if (v < 0) {
        ASSERT(!"negative wait_group counter");
    }

    if (w != 0 && delta > 0 && v == delta) {
        ASSERT(!"wait_group misuse: Add called concurrently with Wait");
    }

    if (v > 0 || w == 0) {
        return;
    }

    // This thread has set counter to 0 when waiters > 0.
    // Now there can't be concurrent mutations of state:
    // - Adds must not happen concurrently with Wait,
    // - Wait does not increment waiters if it sees counter == 0.
    // Still do a cheap sanity check to detect wait group misuse.
    if (wg->state != state) {
        ASSERT(!"wait_group misuse: Add called concurrently with Wait");
    }

    // reset waiters counter to zero
    wg->state = 0;
    for (; w != 0; w--) {
        semaphore_release(&wg->sema, false);
    }
}

void wait_group_done(wait_group_t* wg) {
    wait_group_add(wg, -1);
}

void wait_group_wait(wait_group_t* wg) {
    while (true) {
        uint64_t state = wg->state;
        int32_t v = (int32_t)(state >> 32);

        if (v == 0) {
            // counter is zero, no need to wait
            return;
        }

        // Increment waiters count
        if (atomic_compare_exchange_strong(&wg->state, &state, state + 1)) {
            semaphore_acquire(&wg->sema, false, -1);

            if (wg->state != 0) {
                ASSERT("wait_group is reused before previous wait has returned");
            }

            return;
        }
    }
}
