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

#include <stdatomic.h>
#include "mutex.h"
#include "util/except.h"
#include "proc/scheduler.h"
#include "time/timer.h"

typedef enum mutex_state {
    MUTEX_LOCKED = 1 << 0,
    MUTEX_WOKEN = 1 << 1,
    MUTEX_STARVING = 1 << 2,
} mutex_state_t;

#define MUTEX_WAITER_SHIFT 3

#define STARVATION_THRESHOLD_US 1000

static void mutex_lock_slow(mutex_t* mutex) {
    int64_t wait_start_time;
    bool starving = false;
    bool awoke = false;
    int iter = 0;
    int32_t old = mutex->state;
    while (true) {
        // Don't spin in starvation mode, ownership is handed off to waiters
        // so we won't be able to acquire the mutex anyway.
        if ((old & (MUTEX_LOCKED | MUTEX_STARVING)) == MUTEX_LOCKED && scheduler_can_spin(iter)) {
            // Active spinning makes sense.
            // Try to set mutexWoken flag to inform Unlock
            // to not wake other blocked goroutines.
            if (!awoke && (old & MUTEX_WOKEN) == 0 && (old >> MUTEX_WAITER_SHIFT) != 0 && atomic_compare_exchange_weak(&mutex->state, &old, old | MUTEX_WOKEN)) {
                awoke = true;
            }

            // spin a little
            for (int i = 0; i < 30; i++) {
                __builtin_ia32_pause();
            }

            iter++;
            continue;
        }

        int32_t new = old;

        // Don't try to acquire starving mutex, new arriving threads must queue.
        if ((old & MUTEX_STARVING) == 0) {
            new |= MUTEX_LOCKED;
        }
        if ((old & (MUTEX_LOCKED | MUTEX_STARVING)) != 0) {
            new += 1 << MUTEX_WAITER_SHIFT;
        }

        // The current thread switches mutex to starvation mode.
        // But if the mutex is currently unlocked, don't do the switch.
        // Unlock expects that starving mutex has waiters, which will not
        // be true in this case.
        if (starving && (old & MUTEX_LOCKED) != 0) {
            new |= MUTEX_STARVING;
        }

        if (awoke) {
            // The thread has been woken from sleep,
            // so we need to reset the flag in either case.
            new &= ~MUTEX_WOKEN;
        }

        int32_t told = old;
        if (atomic_compare_exchange_weak(&mutex->state, &told, new)) {
            if ((old & (MUTEX_LOCKED | MUTEX_STARVING)) == 0) {
                // locked the mutex with cas
                break;
            }

            // If we were already waiting before, queue at the front of the queue.
            bool queue_lifo = wait_start_time != 0;
            if (wait_start_time == 0) {
                wait_start_time = microtime();
            }
            semaphore_acquire(&mutex->semaphore, queue_lifo);
            starving = starving || microtime() - wait_start_time > STARVATION_THRESHOLD_US;
            old = mutex->state;
            if ((old & MUTEX_STARVING) != 0) {
                // If this thread was woken and mutex is in starvation mode,
                // ownership was handed off to us but mutex is in somewhat
                // inconsistent state: MUTEX_LOCKED is not set and we are still
                // accounted as waiter. Fix that.
                int32_t delta = MUTEX_LOCKED - (1 << MUTEX_WAITER_SHIFT);
                if (!starving || (old >> MUTEX_WAITER_SHIFT) == 1) {
                    // Exit starvation mode.
                    // Critical to do it here and consider wait time.
                    // Starvation mode is so inefficient, that two goroutines
                    // can go lock-step infinitely once they switch mutex
                    // to starvation mode.
                    delta -= MUTEX_STARVING;
                }
                atomic_fetch_add(&mutex->state, delta);
                break;
            }
            awoke = true;
            iter = 0;
        }
    }
}

void mutex_lock(mutex_t* mutex) {
    // Fast path: grab unlocked mutex.
    int32_t zero = 0;
    if (atomic_compare_exchange_weak(&mutex->state, &zero, MUTEX_LOCKED)) {
        return;
    }

    // Slow path (outlined so that the fast path can be inlined)
    mutex_lock_slow(mutex);
}

bool mutex_try_lock(mutex_t* mutex) {
    int32_t old = mutex->state;
    if ((old & (MUTEX_LOCKED | MUTEX_STARVING)) != 0) {
        return false;
    }

    // There may be a thread waiting for the mutex, but we are
    // running now and can try to grab the mutex before that
    // thread wakes up.
    if (!atomic_compare_exchange_strong(&mutex->state, &old, old | MUTEX_LOCKED)) {
        return false;
    }

    return true;
}

static void mutex_unlock_slow(mutex_t* mutex, int32_t new) {
    ASSERT((new + MUTEX_LOCKED) & MUTEX_LOCKED);

    if ((new & MUTEX_STARVING) == 0) {
        int32_t old = new;
        while (true) {
            // If there are no waiters or a thread has already
            // been woken or grabbed the lock, no need to wake anyone.
            // In starvation mode ownership is directly handed off from unlocking
            // thread to the next waiter. We are not part of this chain,
            // since we did not observe MUTEX_STARVING when we unlocked the mutex above.
            // So get off the way.
            if ((old >> MUTEX_WAITER_SHIFT) == 0 || (old & (MUTEX_LOCKED | MUTEX_WOKEN | MUTEX_STARVING)) != 0) {
                return;
            }

            // Grab the right to wake someone.
            new = (old - (1 << MUTEX_WAITER_SHIFT)) | MUTEX_WOKEN;
            if (atomic_compare_exchange_weak(&mutex->state, &old, new)) {
                semaphore_release(&mutex->semaphore, false);
                return;
            }
        }
    } else {
        // Starving mode: handoff mutex ownership to the next waiter, and yield
        // our time slice so that the next waiter can start to run immediately.
        // Note: MUTEX_LOCKED is not set, the waiter will set it after wakeup.
        // But mutex is still considered locked if MUTEX_STARVING is set,
        // so new coming threads won't acquire it.
        semaphore_release(&mutex->semaphore, true);
    }
}

void mutex_unlock(mutex_t* mutex) {
    // Fast path: drop lock bit.
    int32_t new = atomic_fetch_sub(&mutex->state, MUTEX_LOCKED) - MUTEX_LOCKED;
    if (new != 0) {
        // Outlined slow path to allow inlining the fast path.
        // To hide unlockSlow during tracing we skip one extra frame when tracing GoUnblock.
        mutex_unlock_slow(mutex, new);
    }
}
