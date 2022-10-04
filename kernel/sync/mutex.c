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

#include "mutex.h"
#include "thread/waitable.h"
#include "time/delay.h"

#include <thread/scheduler.h>
#include <util/except.h>
#include <time/tsc.h>

#include <stdatomic.h>

typedef enum mutex_state {
    MUTEX_LOCKED = 1 << 0,
    MUTEX_WOKEN = 1 << 1,
    MUTEX_STARVING = 1 << 2,
} mutex_state_t;

#define MUTEX_WAITER_SHIFT 3

#define STARVATION_THRESHOLD_US 1000

static void mutex_lock_slow(mutex_t* mutex) {
    int64_t wait_start_time = 0;
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
            if (
                !awoke && (old & MUTEX_WOKEN) == 0 && (old >> MUTEX_WAITER_SHIFT) != 0 &&
                atomic_compare_exchange_weak(&mutex->state, &old, old | MUTEX_WOKEN)
            ) {
                awoke = true;
            }

            // spin a little
            for (int i = 0; i < 30; i++) {
                __builtin_ia32_pause();
            }

            iter++;
            old = mutex->state;
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
            ASSERT(new & MUTEX_WOKEN && "inconsistent mutex state");
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
                if ((old & (MUTEX_LOCKED | MUTEX_WOKEN)) != 0 || (old >> MUTEX_WAITER_SHIFT) == 0) {
                    ASSERT(!"inconsistent mutex state");
                }
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
        } else {
            // update the old value
            old = mutex->state;
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
    ASSERT((new + MUTEX_LOCKED) & MUTEX_LOCKED && "unlock of unlocked mutex");

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
            old = mutex->state;
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Self test
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void hammer_mutex(mutex_t* m, int loops, waitable_t* wdone) {
    for (int i = 0; i < loops; i++) {
        if (i % 3 == 0) {
            if (mutex_try_lock(m)) {
                mutex_unlock(m);
            }
            continue;
        }
        mutex_lock(m);
        mutex_unlock(m);
    }
    waitable_send(wdone, true);
    release_waitable(wdone);
}

static void test_mutex() {
    TRACE("\t\tTest mutex");

    mutex_t m = { 0 };
    mutex_lock(&m);
    ASSERT(!mutex_try_lock(&m) && "mutex_try_lock succeeded with mutex locked");
    mutex_unlock(&m);
    ASSERT(mutex_try_lock(&m) && "mutex_try_lock failed with mutex unlocked");
    mutex_unlock(&m);

    waitable_t* w = create_waitable(0);

    for (int i = 0; i < 10; i++) {
        thread_t* t = create_thread((void*)hammer_mutex, NULL, "test-%d", i);
        t->save_state.rdi = (uintptr_t)&m;
        t->save_state.rsi = 1000;
        t->save_state.rdx = (uintptr_t) put_waitable(w);
        scheduler_ready_thread(t);
    }

    for (int i = 0; i < 10; i++) {
        ASSERT(waitable_wait(w, true) == WAITABLE_SUCCESS);
    }

    waitable_close(w);
    release_waitable(w);
}

static void mutex_self_test_lock_for_long(mutex_t* mu, waitable_t* stop) {
    while (true) {
        mutex_lock(mu);
        microdelay(100);
        mutex_unlock(mu);

        waitable_t* ws[] = { stop };
        selected_waitable_t selected = waitable_select(ws, 0, 1, false);
        if (selected.index == 0) {
            release_waitable(stop);
            return;
        }
    }
}

static void mutex_self_test_sleep_lock(mutex_t* mu, waitable_t* done) {
    for (int i = 0; i < 10; i++) {
        microdelay(100);
        mutex_lock(mu);
        mutex_unlock(mu);
    }

    ASSERT(waitable_send(done, true));
    release_waitable(done);
}

static void test_mutex_fairness() {
    TRACE("\t\tTest mutex fairness");

    mutex_t mu = { 0 };
    waitable_t* stop = create_waitable(0);

    thread_t* t = create_thread((void*)mutex_self_test_lock_for_long, NULL, "test1");
    t->save_state.rdi = (uintptr_t)&mu;
    t->save_state.rsi = (uintptr_t) put_waitable(stop);
    scheduler_ready_thread(t);

    waitable_t* done = create_waitable(1);

    t = create_thread((void*)mutex_self_test_sleep_lock, NULL, "test2");
    t->save_state.rdi = (uintptr_t)&mu;
    t->save_state.rsi = (uintptr_t) put_waitable(done);
    scheduler_ready_thread(t);

    waitable_t* ws[] = { done, after(10 * MICROSECONDS_PER_SECOND) };
    selected_waitable_t selected = waitable_select(ws, 0, 2, true);
    ASSERT(selected.index == 0 && "can't acquire Mutex in 10 seconds");

    waitable_close(stop);
    release_waitable(stop);

    release_waitable(done);
}

void mutex_self_test() {
    TRACE("\tMutex self-test");
    test_mutex();
    test_mutex_fairness();
}
