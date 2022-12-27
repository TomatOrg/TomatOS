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

#include "semaphore.h"
#include "thread/waitable.h"
#include "thread/timer.h"
#include "time/tsc.h"
#include "time/tick.h"

#include <thread/scheduler.h>

#include <stdatomic.h>

void semaphore_queue(semaphore_t* semaphore, waiting_thread_t* wt, bool lifo) {
    waiting_thread_t* t = semaphore->waiters;

    if (t == NULL) {
        semaphore->waiters = wt;
    } else {
        if (lifo) {
            // Substitute wt in t's place in waiters.
            semaphore->waiters = wt;
            wt->ticket = t->ticket;
            wt->wait_link = t;
            wt->wait_tail = t->wait_tail;
            if (wt->wait_tail == NULL) {
                wt->wait_tail = t;
            }
            t->wait_tail = NULL;
        } else {
            // Add wt to the end of t's wait list
            if (t->wait_tail == NULL) {
                t->wait_link = wt;
            } else {
                t->wait_tail->wait_link = wt;
            }
            t->wait_tail = wt;
            wt->wait_link = NULL;
        }
    }
}

static void semaphore_remove_wt(semaphore_t* semaphore, waiting_thread_t* wt) {
    if (wt->wait_link != NULL) {
        waiting_thread_t* t = wt->wait_link;
        // Substitute t, for wt in the root
        semaphore->waiters = t;
        t->ticket = wt->ticket;
        if (t->wait_link != NULL) {
            t->wait_tail = wt->wait_tail;
        } else {
            t->wait_tail = NULL;
        }
        wt->wait_link = NULL;
        wt->wait_tail = NULL;
    } else {
        semaphore->waiters = NULL;
    }

}

waiting_thread_t* semaphore_dequeue(semaphore_t* semaphore) {
    waiting_thread_t* wt = semaphore->waiters;
    if (wt == NULL) {
        return NULL;
    }

    semaphore_remove_wt(semaphore, wt);

    wt->ticket = 0;
    return wt;
}


static bool semaphore_can_acquire(semaphore_t* semaphore) {
    while (true) {
        uint32_t v = atomic_load(&semaphore->value);
        if (v == 0) {
            return false;
        }
        if (atomic_compare_exchange_weak(&semaphore->value, &v, v - 1)) {
            return true;
        }
    }
}

typedef struct semaphore_timeout_context {
    semaphore_t* sm;
    waiting_thread_t* wt;
} semaphore_timeout_context_t;

static void semaphore_acquire_timeout(semaphore_timeout_context_t* ctx, uint64_t now) {
    // fast path, check if the waiting thread was already dequeued
    if (ctx->wt->wait_link == NULL) {
        return;
    }

    // slow path, lock the semaphore and dequeue the waiting thread if needed

    spinlock_lock(&ctx->sm->lock);

    // make sure this wt is not awake yet, and wake it up but with timeout
    if (ctx->wt->wait_link != NULL) {
        // remove from the semaphore waiting list
        semaphore_remove_wt(ctx->sm, ctx->wt);

        // mark the ticket as a timeout
        ctx->wt->ticket = -1;

        // ready the thread
        scheduler_ready_thread(ctx->wt->thread);
    }

    spinlock_unlock(&ctx->sm->lock);
}

bool semaphore_acquire(semaphore_t* semaphore, bool lifo, int64_t timeout) {
    bool acquired = false;

    // Easy case
    if (semaphore_can_acquire(semaphore)) {
        return true;
    }

    // we have a zero timeout, return right now
    if (timeout == 0) {
        return false;
    }

    // Harder case:
    //	increment waiter count
    //	try semaphore_can_acquire one more time, return if succeeded
    //	enqueue itself as a waiter
    //	sleep
    //	(waiter descriptor is dequeued by signaler)
    waiting_thread_t* wt = acquire_waiting_thread();
    wt->thread = get_current_thread();

    while (true) {
        spinlock_lock(&semaphore->lock);

        // Add ourselves to nwait to disable "easy case" in semaphore_release
        atomic_fetch_add(&semaphore->nwait, 1);

        // Check semaphore_can_acquire to avoid missed wakeup
        if (semaphore_can_acquire(semaphore)) {
            atomic_fetch_sub(&semaphore->nwait, 1);
            spinlock_unlock(&semaphore->lock);
            break;
        }

        // Any semaphore_release after the semaphore_can_acquire knows we're waiting
        // (we set nwait above), so go to sleep.
        semaphore_queue(semaphore, wt, lifo);

        // if we have a timeout then prepare a timer for it
        timer_t* timer = NULL;
        semaphore_timeout_context_t ctx;
        if (timeout > 0) {
            timer = create_timer();
            ctx = (semaphore_timeout_context_t){
                .wt = wt,
                .sm = semaphore
            };
            timer->arg = &ctx;
            timer->func = (timer_func_t) semaphore_acquire_timeout;
            timer->when = (int64_t) get_tick() + timeout;
            timer_start(timer);
        }

        // park the thread, making sure to unlock our lock
        scheduler_park((void*)spinlock_unlock, &semaphore->lock);

        // cleanup the timeout timer
        if (timer != NULL) {
            timer_stop(timer);
            release_timer(timer);
        }

        // check if we got a timeout, if so break
        if (wt->ticket == -1) {
            break;
        }

        if (wt->ticket != 0 || semaphore_can_acquire(semaphore)) {
            acquired = true;
            break;
        }
    }

    release_waiting_thread(wt);

    return acquired;
}

void semaphore_release(semaphore_t* semaphore, bool handoff) {
    atomic_fetch_add(&semaphore->value, 1);

    // Easy case: no waiters?
    // This check must happen after the add, to avoid a missed wakeup
    // (see loop in semaphore_acquire).
    if (atomic_load(&semaphore->nwait) == 0) {
        return;
    }

    // Harder case: search for a waiter and wake it.
    spinlock_lock(&semaphore->lock);
    if (atomic_load(&semaphore->nwait) == 0) {
        // The count is already consumed by another thread,
        // so no need to wake up another thread.
        spinlock_unlock(&semaphore->lock);
        return;
    }

    waiting_thread_t* wt = semaphore_dequeue(semaphore);
    if (wt != NULL) {
        atomic_fetch_sub(&semaphore->nwait, 1);
    }
    spinlock_unlock(&semaphore->lock);

    // May be slow or even yield, so unlock first
    if (wt != NULL) {
        ASSERT(wt->ticket == 0 && "corrupted semaphore ticket");

        if (handoff && semaphore_can_acquire(semaphore)) {
            wt->ticket = 1;
        }

        scheduler_ready_thread(wt->thread);

        if (wt->ticket == 1) {
            // Direct thread handoff
            // scheduler_ready_thread has added the waiter thread as run next in the
            // current cpu, we now call the scheduler so that we start running the
            // waiter thread immediately.
            // Note that the waiter inherits our time slice: this is desirable to avoid
            // having a highly contended semaphore hog the cpu indefinitely. scheduler_yield
            // is like scheduler_schedule, but it puts the current thread on the local run queue
            // instead of the global one. We only do this in the starving regime (handoff=true),
            // as in non-starving case it is possible for a different waiter to acquire the semaphore
            // while we are yielding/scheduling, and this would be wasteful. We wait instead to enter
            // starving regime, and then we do direct handoffs of ticket and cpu.
            scheduler_yield();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Self test
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void hammer_semaphore(semaphore_t* s, int loops, waitable_t* wdone) {
    for (int i = 0; i < loops; i++) {
        semaphore_acquire(s, false, -1);
        semaphore_release(s, false);
    }
    waitable_send(wdone, true);
    release_waitable(wdone);
}

static void test_semaphore() {
    semaphore_t s = { .value = 1 };
    waitable_t* w = create_waitable(0);

    for (int i = 0; i < 10; i++) {
        thread_t* t = create_thread((void*)hammer_semaphore, NULL, "test-%d", i);
        t->save_state.rdi = (uintptr_t)&s;
        t->save_state.rsi = 1000;
        t->save_state.rdx = (uintptr_t) put_waitable(w);
        scheduler_ready_thread(t);
    }

    for (int i = 0; i < 10; i++) {
        waitable_wait(w, true);
    }

    waitable_close(w);
    release_waitable(w);
}

void semaphore_self_test() {
    TRACE("\tSemaphore self-test");
    test_semaphore();
}
