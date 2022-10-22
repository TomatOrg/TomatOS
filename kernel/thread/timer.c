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

#include "timer.h"

#include "scheduler.h"
#include "cpu_local.h"
#include "util/stb_ds.h"
#include "time/tsc.h"

#include <mem/malloc.h>

#include <stdatomic.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Timer subsystem
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct timers {
    // Lock for timers. We normally access the timers while running
    // on this CPU, but the scheduler can also do it from a different CPU.
    spinlock_t timers_lock;

    // Actions to take at some time. This is used to implement
    // the standard library's System.Threading.Timer class
    timer_t** timers;

    // Number of timers in the CPU's heap.
    _Atomic(uint32_t) num_timers;

    // Number of deleted timers in CPU's heap.
    _Atomic(uint32_t) deleted_timers;

    // The when field of the first entry on the timer heap.
    // This is updated using atomic functions.
    // This is 0 if the timer heap is empty.
    _Atomic(int64_t) timer0_when;

    // The earliest known nextwhen field of a timer
    // with a timer modified earlier status. Because
    // the timer may have been modified again, there need not
    // be any timer with this value.
    // This is updated using atomic functions.
    // This is 0 if there are no timer modified earlier timers
    _Atomic(int64_t) timer_modified_earliest;
} timers_t;

/**
 * The local timers context
 */
static timers_t CPU_LOCAL m_timers;

static timers_t* get_timers() {
    return get_cpu_local_base(&m_timers);
}

//----------------------------------------------------------------------------------------------------------------------
// Timer heap management
//----------------------------------------------------------------------------------------------------------------------

/**
 * Puts the timer at position i in the right place
 * in the heap, by moving it up toward the top of the heap.
 *
 * @return The smallest changed index
 */
INTERRUPT static int siftup_timer(timer_t** timers, int i) {
    ASSERT(i < arrlen(timers));

    int64_t when = timers[i]->when;
    ASSERT(when > 0);

    timer_t* tmp = timers[i];
    while (i > 0) {
        int parent = (i - 1) / 4;
        if (when >= timers[parent]->when) {
            break;
        }

        timers[i] = timers[parent];
        i = parent;
    }

    if (tmp != timers[i]) {
        timers[i] = tmp;
    }

    return i;
}

/**
 * Puts the timer at position i in the right place
 * in the heap by moving it down towards the bottom of the heap.
 */
INTERRUPT static void siftdown_timer(timer_t** timers, int i) {
    int n = arrlen(timers);
    ASSERT(i < n);

    int64_t when = timers[i]->when;
    ASSERT(when > 0);

    timer_t* tmp = timers[i];
    while (true) {
        int left_child = i * 4 + 1;
        int mid_child = left_child + 2;
        if (left_child >= n) {
            break;
        }

        int64_t left_when = timers[left_child]->when;
        if (left_child + 1 < n && timers[left_child + 1]->when < left_when) {
            left_when = timers[left_child + 1]->when;
            left_child++;
        }

        if (mid_child < n) {
            int64_t mid_when = timers[mid_child]->when;
            if (mid_child + 1 < n && timers[mid_child + 1]->when < mid_when) {
                mid_when = timers[mid_child + 1]->when;
                mid_child++;
            }

            if (mid_when < left_when) {
                left_when = mid_when;
                left_child = mid_child;
            }
        }

        if (left_when >= when) {
            break;
        }

        timers[i] = timers[left_child];
        i = left_child;
    }

    if (tmp != timers[i]) {
        timers[i] = tmp;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// High level timer management
//----------------------------------------------------------------------------------------------------------------------

INTERRUPT static void update_timer0_when(timers_t* timers) {
    if (arrlen(timers->timers) == 0) {
        atomic_store(&timers->timer0_when, 0);
    } else {
        atomic_store(&timers->timer0_when, timers->timers[0]->when);
    }
}

INTERRUPT static void update_timer_modified_earliest(timers_t* timers, int64_t next_when) {
    while (true) {
        int64_t old = atomic_load(&timers->timer_modified_earliest);
        if (old != 0 && old < next_when) {
            return;
        }
        if (atomic_compare_exchange_strong(&timers->timer_modified_earliest, &old, next_when)) {
            return;
        }
    }
}

/**
 * Removes the first timer from the current CPU's heap.
 */
INTERRUPT static void do_delete_timer0(timers_t* timers) {
    timer_t* timer = timers->timers[0];
    ASSERT(timer->timers == timers);

    timer->timers = NULL;

    // pop it, and replace from the back if there is still items
    timer_t* last = arrpop(timers->timers);
    if (arrlen(timers->timers) > 0) {
        timers->timers[0] = last;
    }

    // sort out the heap
    if (arrlen(timers->timers) > 0) {
        siftdown_timer(timers->timers, 0);
    }
    update_timer0_when(timers);

    // we have one less timer
    atomic_fetch_sub(&timers->num_timers, 1);
}

INTERRUPT static void do_add_timer(timers_t* timers, timer_t* timer) {
    ASSERT(timer->timers == timers);
    timer->timers = get_cpu_local_base(&m_timers);

    // push the timer, make sure to update the ref count
    int i = arrlen(timers->timers);
    arrpush(timers->timers, put_timer(timer));

    // place the timer in the correct place
    siftup_timer(timers->timers, i);

    // update the timer
    if (timer == timers->timers[0]) {
        atomic_store(&timers->timer0_when, timer->when);
    }
    atomic_fetch_add(&timers->num_timers, 1);
}

/**
 * Removes the timer from the current CPU's heap.
 */
static int do_delete_timer(timers_t* timers, int i) {
    timer_t* timer = timers->timers[i];
    ASSERT(timer->timers == timers);

    timer->timers = NULL;

    int last = arrlen(timers->timers) - 1;
    if (i != last) {
        timers->timers[i] = timers->timers[last];
    }
    arrsetlen(timers->timers, last);

    int smallest_changed = i;
    if (i != last) {
        smallest_changed = siftup_timer(timers->timers, i);
        siftdown_timer(timers->timers, i);
    }

    if (i == 0) {
        update_timer0_when(timers);
    }

    atomic_fetch_sub(&timers->num_timers, 1);

    return smallest_changed;
}

/**
 * Cleans up the head of the timer queue, This speeds up
 * programs that create and delete timers, leaving them in
 * the heap slows down add_timer.
 */
static void clean_timers(timers_t* timers) {
    while (true) {
        if (arrlen(timers->timers) == 0) {
            return;
        }

        // This loop can theoretically run for a while, and because
        // it is holding timers_lock it cannot be preempted. If
        // someone is trying to preempt us, just return.
        // We can clean the timers later
        if (get_current_thread()->preempt_stop) {
            return;
        }

        timer_t* timer = timers->timers[0];
        ASSERT(timer->timers == timers);

        timer_status_t status = atomic_load(&timer->status);
        switch (status) {
            case TIMER_DELETED: {
                if (!atomic_compare_exchange_strong(&timer->status, &status, TIMER_REMOVING)) {
                    continue;
                }

                do_delete_timer0(timers);

                status = TIMER_REMOVING;
                ASSERT(atomic_compare_exchange_strong(&timer->status, &status, TIMER_REMOVED));
                SAFE_RELEASE_TIMER(timer);

                atomic_fetch_sub(&timers->deleted_timers, 1);
            } break;

            case TIMER_MODIFIED_EARLIER:
            case TIMER_MODIFIED_LATER: {
                if (!atomic_compare_exchange_strong(&timer->status, &status, TIMER_MOVING)) {
                    continue;
                }

                // Now we can change the when field
                timer->when = timer->nextwhen;

                // Move timer to the right position
                do_delete_timer0(timers);
                do_add_timer(timers, timer);

                status = TIMER_MOVING;
                ASSERT(!atomic_compare_exchange_strong(&timer->status, &status, TIMER_WAITING));
            } break;

            default:
                // Head of timers does not need adjustment
                return;
        }
    }
}

/**
 * Adds any timers we adjusted in adjust_timers back
 * to the timer heap.
 */
static void add_adjusted_timers(timers_t* timers, timer_t** moved) {
    for (int i = 0; i < arrlen(moved); i++) {
        timer_t* timer = moved[i];
        do_add_timer(timers, timer);
        timer_status_t status = TIMER_MOVING;
        ASSERT(atomic_compare_exchange_strong(&timer->status, &status, TIMER_WAITING));
    }
}

/**
 * Looks through the timers in the current CPU's heap for
 * any timers that have been modified to run earlier, and
 * puts them in the correct place in the heap. While looking
 * for those timers, it also moves timers that have been
 * modified to run later, and removes deleted timers. The
 * caller must have locked the timers for CPU.
 */
INTERRUPT static void adjust_timers(timers_t* timers, int64_t now) {
    // If we haven't yet reached the time of the first timer_modified_earlier
    // timer, don't do anything. This speeds up programs that adjust
    // a lot of timers back and forth if the timers rarely expire.
    // We'll postpone looking through all the adjusted timers until
    // one would actually expire
    int64_t first = atomic_load(&timers->timer_modified_earliest);
    if (first == 0 || first > now) {
        return;
    }

    // We are going to clear all timer modified earlier timers
    atomic_store(&timers->timer_modified_earliest, 0);

    timer_t** moved = NULL;
    for (int i = 0; i < arrlen(timers->timers); i++) {
        timer_t* timer = timers->timers[i];

        timer_status_t status = atomic_load(&timer->status);
        switch (status) {
            case TIMER_DELETED: {
                if (atomic_compare_exchange_strong(&timer->status, &status, TIMER_REMOVING)) {
                    int changed = do_delete_timer(timers, i);

                    status = TIMER_REMOVING;
                    ASSERT(atomic_compare_exchange_strong(&timer->status, &status, TIMER_REMOVED));
                    SAFE_RELEASE_TIMER(timer);

                    atomic_fetch_sub(&timers->deleted_timers, 1);

                    // Go back to the earliest changed heap entry.
                    // - 1 because the loop will add 1
                    i = changed - 1;
                }
            } break;

            case TIMER_MODIFIED_EARLIER:
            case TIMER_MODIFIED_LATER: {
                if (atomic_compare_exchange_strong(&timer->status, &status, TIMER_MOVING)) {
                    // Now we can change the when field
                    timer->when = timer->nextwhen;

                    // Take timer of f the heap, and hold onto it.
                    // We don't add it back yet because the heap
                    // manipulation could cause our loop to skip
                    // some other timer.
                    int changed = do_delete_timer(timers, i);
                    arrpush(moved, timer);

                    // Go back to the earliest changed heap entry.
                    // - 1 because the loop will add 1
                    i = changed - 1;
                }
            } break;

            case TIMER_NO_STATUS:
            case TIMER_RUNNING:
            case TIMER_REMOVING:
            case TIMER_REMOVED:
            case TIMER_MOVING: {
                ASSERT(!"Invalid timer status");
            } break;

            case TIMER_WAITING: {
                // OK, nothing to do.
            } break;

            case TIMER_MODIFYING: {
                // Check again after modification is complete.
                scheduler_yield();
                i--;
            } break;

            default:
                ASSERT(!"Invalid timer status");
        }
    }

    if (arrlen(moved) > 0) {
        add_adjusted_timers(timers, moved);
    }

    arrfree(moved);
}

/**
 * Runs a single timer.
 */
INTERRUPT static void run_one_timer(timers_t* timers, timer_t* timer, int64_t now) {
    timer_status_t status = TIMER_RUNNING;

    timer_func_t func = timer->func;
    void* arg = timer->arg;
    uintptr_t seq = timer->seq;

    if (timer->period > 0) {
        // Leave in the heap but adjust next time to fire.
        int64_t delta = timer->when - now;
        timer->when += timer->period * (1 + -delta / timer->period);
        if (timer->when < 0) {
            timer->when = INT64_MAX;
        }
        siftdown_timer(timers->timers, 0);

        // set as waiting now
        ASSERT(atomic_compare_exchange_strong(&timer->status, &status, TIMER_WAITING));

        // update the timers
        update_timer0_when(timers);
    } else {
        // Remove from heap.
        do_delete_timer0(timers);
        ASSERT(atomic_compare_exchange_strong(&timer->status, &status, TIMER_NO_STATUS));
        SAFE_RELEASE_TIMER(timer);
    }

    // run it without the timers lock held
    spinlock_unlock(&timers->timers_lock);

    func(arg, seq);

    spinlock_lock(&timers->timers_lock);
}

/**
 * Examines the first timer in timers. If it is ready based on now,
 * it runs the timer and removes or updates it.
 *
 * Retruns 0 if it ran a timer, -1 if there are no more timers, or the time
 * when the first timer should run.
 */
INTERRUPT static int64_t run_timer(timers_t* timers, int64_t now) {
    while (true) {
        timer_t* timer = timers->timers[0];
        ASSERT(timer->timers == timers);

        timer_status_t status = atomic_load(&timer->status);
        switch (status) {
            case TIMER_WAITING: {
                if (timer->when > now) {
                    return timer->when;
                }

                if (!atomic_compare_exchange_strong(&timer->status, &status, TIMER_RUNNING)) {
                    continue;
                }

                // Note that run_one_timer may temporarily unlock
                // the timers lock
                run_one_timer(timers, timer, now);

                return 0;
            } break;

            case TIMER_DELETED: {
                if (!atomic_compare_exchange_strong(&timer->status, &status, TIMER_REMOVING)) {
                    continue;
                }

                do_delete_timer0(timers);

                status = TIMER_REMOVING;
                ASSERT(atomic_compare_exchange_strong(&timer->status, &status, TIMER_REMOVED));
                SAFE_RELEASE_TIMER(timer);

                atomic_fetch_sub(&timers->deleted_timers, 1);
                if (arrlen(timers->timers) == 0) {
                    return -1;
                }
            } break;

            case TIMER_MODIFIED_EARLIER:
            case TIMER_MODIFIED_LATER: {
                if (!atomic_compare_exchange_strong(&timer->status, &status, TIMER_REMOVING)) {
                    continue;
                }

                do_delete_timer0(timers);
                status = TIMER_REMOVING;
                ASSERT(atomic_compare_exchange_strong(&timer->status, &status, TIMER_REMOVED));
                SAFE_RELEASE_TIMER(timer);

                atomic_fetch_add(&timers->deleted_timers, 1);
                if (arrlen(timers->timers) == 0) {
                    return -1;
                }
            } break;

            case TIMER_MODIFYING: {
                // Wait for modification to complete.
                scheduler_yield();
            } break;

            case TIMER_NO_STATUS:
            case TIMER_REMOVED: {
                ASSERT(!"Should not see a new or inactive timer on the heap");
            } break;

            case TIMER_RUNNING:
            case TIMER_REMOVING:
            case TIMER_MOVING: {
                ASSERT(!"These should only be set when timers are locked, and we didn't do it");
            } break;

            default:
                ASSERT(!"Bad timer status");
        }
    }
}

INTERRUPT static void clear_deleted_timers(timers_t* timers) {
    atomic_store(&timers->timer_modified_earliest, 0);

    int cdel = 0;
    int to = 0;
    bool changed_heap = false;

    for (int i = 0; i < arrlen(timers->timers); i++) {
        timer_t* timer = timers->timers[i];
        while (true) {
            timer_status_t status = atomic_load(&timer->status);
            switch (status) {
                case TIMER_WAITING: {
                    if (changed_heap) {
                        timers->timers[to] = timer;
                        siftup_timer(timers->timers, to);
                    }
                    to++;
                    goto next_timer;
                } break;

                case TIMER_MODIFIED_EARLIER:
                case TIMER_MODIFIED_LATER: {
                    if (atomic_compare_exchange_strong(&timer->status, &status, TIMER_MOVING)) {
                        timer->when = timer->nextwhen;
                        timers->timers[to] = timer;
                        siftup_timer(timers->timers, to);
                        to++;
                        changed_heap = true;
                        status = TIMER_MOVING;
                        ASSERT(atomic_compare_exchange_strong(&timer->status, &status, TIMER_WAITING));
                        goto next_timer;
                    }
                } break;

                case TIMER_DELETED: {
                    if (atomic_compare_exchange_strong(&timer->status, &status, TIMER_REMOVING)) {
                        timer->timers = NULL;
                        cdel++;

                        status = TIMER_REMOVING;
                        ASSERT(atomic_compare_exchange_strong(&timer->status, &status, TIMER_REMOVED));

                        changed_heap = true;
                        goto next_timer;
                    }
                } break;

                case TIMER_MODIFYING: {
                    // Loop until modification complete
                    scheduler_yield();
                } break;

                case TIMER_NO_STATUS:
                case TIMER_REMOVED: {
                    ASSERT(!"We should not see these status values in a timer heap.");
                } break;

                case TIMER_RUNNING:
                case TIMER_REMOVING:
                case TIMER_MOVING: {
                    ASSERT(!"Some other CPU thinks it owns this timer, which should not happen.");
                } break;

                default: ASSERT(!"Invalid timer status");
            }
        }

    next_timer:
        continue;
    }

    // Remove the timer slots from the heap

    for (int i = to; i < arrlen(timers->timers); i++) {
        SAFE_RELEASE_TIMER(timers->timers[i]);
    }

    atomic_fetch_sub(&timers->deleted_timers, cdel);
    atomic_fetch_sub(&timers->num_timers, cdel);

    // remove from the array
    arrsetlen(timers->timers, arrlen(timers->timers) - cdel);

    update_timer0_when(timers);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Timer management
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

timer_t* create_timer() {
    timer_t* timer = malloc(sizeof(timer_t));
    if (timer == NULL) {
        return timer;
    }

    timer->timers = NULL;
    timer->ref_count = 1;
    return timer;
}

void timer_start(timer_t* timer) {
    ASSERT(timer->when > 0 && "timer when must be positive");
    ASSERT(timer->period >= 0 && "timer period must be non-negative");
    ASSERT(timer->status == TIMER_NO_STATUS && "timer_start called with initialized timer");

    atomic_store(&timer->status, TIMER_WAITING);

    int64_t when = timer->when;

    // Disable preemption while using the CPU to avoid changing another CPU's heap.
    scheduler_preempt_disable();

    timers_t* timers = get_timers();
    spinlock_lock(&timers->timers_lock);
    clean_timers(timers);
    do_add_timer(timers, timer);
    spinlock_unlock(&timers->timers_lock);

    scheduler_preempt_enable();

    scheduler_wake_poller(when);
}

bool timer_stop(timer_t* timer) {
    while (true) {
        timer_status_t status = atomic_load(&timer->status);
        switch (status) {
            case TIMER_WAITING:
            case TIMER_MODIFIED_LATER:
            case TIMER_MODIFIED_EARLIER: {
                // prevent preemption while the timer is in TIMER_MODIFYING
                scheduler_preempt_disable();
                if (atomic_compare_exchange_strong(&timer->status, &status, TIMER_MODIFYING)) {
                    // Must fetch cpu before changing status, as clean_timers in another
                    // thread can clear cpu of a TIMER_DELETED timer.
                    timers_t* timers = timer->timers;

                    status = TIMER_MODIFYING;
                    ASSERT(atomic_compare_exchange_strong(&timer->status, &status, TIMER_DELETED));

                    scheduler_preempt_enable();

                    // report that we have marked this as deleted to the given cpu
                    atomic_fetch_add(&timers->deleted_timers, 1);

                    // Timer was not yet run
                    return true;
                } else {
                    scheduler_preempt_enable();
                }
            } break;

            case TIMER_DELETED:
            case TIMER_REMOVING:
            case TIMER_REMOVED: {
                // Timer has already run.
                return false;
            } break;

            case TIMER_RUNNING:
            case TIMER_MOVING: {
                // The timer is being run or moved, by a different CPU.
                // Wait for it to complete
                scheduler_yield();
            } break;

            case TIMER_NO_STATUS: {
                // Removing timer that was never added or has
                // already been run.
                return false;
            } break;

            case TIMER_MODIFYING: {
                // Simultaneous calls to timer_stop and timer_modify.
                // Wait for the other call to complete.
                scheduler_yield();
            } break;

            default: ASSERT(!"Bad timer status");
        }
    }
}

bool timer_reset(timer_t* timer, int64_t when) {
    return timer_modify(timer, when, timer->period, timer->func, timer->arg, timer->seq);
}

bool timer_modify(timer_t* timer, int64_t when, int64_t period, timer_func_t func, void* arg, uintptr_t seq) {
    ASSERT(when > 0);
    ASSERT(period >= 0);

    bool was_removed = false;
    bool pending = false;

    while (true) {
        timer_status_t status = atomic_load(&timer->status);
        switch (status) {
            case TIMER_WAITING:
            case TIMER_MODIFIED_EARLIER:
            case TIMER_MODIFIED_LATER: {
                // Prevent preemption while the timer is in TIMER_MODIFYING
                scheduler_preempt_disable();
                if (atomic_compare_exchange_strong(&timer->status, &status, TIMER_MODIFYING)) {
                    pending = true; // timer not yet run
                    goto got_timer;
                }
                scheduler_preempt_enable();
            } break;

            case TIMER_NO_STATUS:
            case TIMER_REMOVED: {
                // Prevent preemption while the timer is in TIMER_MODIFYING
                scheduler_preempt_disable();

                // Timer was already run, and timer is no longer in a heap.
                // Act like timer_start
                if (atomic_compare_exchange_strong(&timer->status, &status, TIMER_MODIFYING)) {
                    was_removed = true;
                    pending = false; // timer already run or stopped;
                    goto got_timer;
                }

                scheduler_preempt_enable();
            } break;

            case TIMER_DELETED: {
                // Prevent preemption while the timer is in TIMER_MODIFYING
                scheduler_preempt_disable();
                if (atomic_compare_exchange_strong(&timer->status, &status, TIMER_MODIFYING)) {
                    timers_t* timers = timer->timers;
                    atomic_fetch_sub(&timers->deleted_timers, 1);
                    pending = false; // timer already stopped
                    goto got_timer;
                }
                scheduler_preempt_enable();
            } break;

            case TIMER_RUNNING:
            case TIMER_REMOVING:
            case TIMER_MOVING: {
                // The timer is being run or moved, by a different CPU.
                // Wait for it to complete
                scheduler_yield();
            } break;

            case TIMER_MODIFYING: {
                // Multiple simultaneous calls to timer_modify.
                // Wait for the other call to complete
                scheduler_yield();
            } break;

            default: ASSERT(!"Bad timer status");
        }
    }
got_timer:

    timer->period = period;
    timer->func = func;
    timer->arg = arg;
    timer->seq = seq;

    if (was_removed) {
        timer->when = when;

        // add the timer back
        timers_t* timers = get_timers();
        spinlock_lock(&timers->timers_lock);
        do_add_timer(timers, timer);
        spinlock_unlock(&timers->timers_lock);

        // update the status
        timer_status_t status = TIMER_MODIFYING;
        ASSERT (atomic_compare_exchange_strong(&timer->status, &status, TIMER_WAITING));

        scheduler_preempt_disable();

        scheduler_wake_poller(when);
    } else {
        // The timer is in some other CPU's heap, so we can't change
        // the when field. If we did, the other CPU's heap would be
        // out of order. So we put the new when value in the nextwhen
        // field, and let the other CPU set the when field
        // when it is prepared to resort the heap
        timer->nextwhen = when;

        timer_status_t new_status = when < timer->when ? TIMER_MODIFIED_EARLIER : TIMER_MODIFIED_LATER;

        // get the timers of the given cpu
        if (new_status == TIMER_MODIFIED_EARLIER) {
            timers_t* timers = timer->timers;
            update_timer_modified_earliest(timers, when);
        }

        timer_status_t status = TIMER_MODIFYING;
        ASSERT(atomic_compare_exchange_strong(&timer->status, &status, new_status));

        scheduler_preempt_disable();

        // If the new status is earlier, wake up the poller.
        if (new_status == TIMER_MODIFIED_EARLIER) {
            scheduler_wake_poller(when);
        }
    }

    return pending;
}

timer_t* put_timer(timer_t* timer) {
    atomic_fetch_add(&timer->ref_count, 1);
    return timer;
}

INTERRUPT void release_timer(timer_t* timer) {
    if (atomic_fetch_sub(&timer->ref_count, 1) == 1) {
        // we lost all references, must be already stopped
        ASSERT(timer->status == TIMER_REMOVED || timer->status == TIMER_DELETED);

        // this was the last ref, delete
        free(timer);
    }
}

INTERRUPT void check_timers(int cpu, int64_t* out_now, int64_t* poll_until, bool* ran) {
    timers_t* timers = get_cpu_base(cpu, &m_timers);
    int64_t now = *out_now;

    // If it's not yet time for the first timer, or the first adjusted
    // timer, then there is nothing to do.
    int64_t next = atomic_load(&timers->timer0_when);
    int64_t next_adj = atomic_load(&timers->timer_modified_earliest);

    if (next == 0 || (next_adj != 0 && next_adj < next)) {
        next = next_adj;
    }

    if (next == 0) {
        *poll_until = 0;
        if (ran) *ran = false;
        return;
    }

    if (now == 0) {
        now = (int64_t)microtime();
        *out_now = now;
    }

    if (now < next) {
        // Next timer is not ready to run, but keep going
        // if we would clear deleted timers.
        // This corresponds to the condition below where
        // we decide whether to call clear_deleted_timers.
        if (get_cpu_id() != cpu || atomic_load(&timers->deleted_timers) <= atomic_load(&timers->num_timers) / 4) {
            *poll_until = next;
            if (ran) *ran = false;
            return;
        }
    }

    spinlock_lock(&timers->timers_lock);

    if (arrlen(timers->timers) > 0) {
        adjust_timers(timers, now);

        while (arrlen(timers->timers) > 0) {
            // Note that run_timer may temporarily unlock
            // timers_lock

            int64_t timer_when = run_timer(timers, now);
            if (timer_when != 0) {
                if (timer_when > 0) {
                    *poll_until = timer_when;
                }
                break;
            }

            if (ran) *ran = true;
        }
    }

    // If this is the local CPU, and there are a lot of deleted timers,
    // clear them out. We only do this for the local CPU to reduce the
    // lock contention on timers_lock
    if (cpu == get_cpu_id() && atomic_load(&timers->deleted_timers) <= atomic_load(&timers->num_timers) / 4) {
        clear_deleted_timers(timers);
    }

    spinlock_unlock(&timers->timers_lock);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Bookkeeping
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// cpus with timers
static _Atomic(size_t) m_timer_cpus[256 / (sizeof(size_t) * 8)];

bool cpu_has_timers(int cpu) {
    return mask_read(m_timer_cpus, cpu);
}

void set_has_timers(int cpu) {
    mask_set(m_timer_cpus, cpu);
}

void update_cpu_timers_mask() {
    timers_t* timers = get_timers();

    if (atomic_load(&timers->num_timers) > 0) {
        return;
    }

    spinlock_lock(&timers->timers_lock);
    if (atomic_load(&timers->num_timers) == 0) {
        mask_clear(m_timer_cpus, get_cpu_id());
    }
    spinlock_unlock(&timers->timers_lock);
}

int64_t nobarrier_wake_time(int cpu) {
    timers_t* timers = get_cpu_base(cpu, &m_timers);

    int64_t next = atomic_load(&timers->timer0_when);
    int64_t next_adj = atomic_load(&timers->timer_modified_earliest);
    if (next == 0 || (next_adj != 0 && next_adj < next)) {
        next = next_adj;
    }
    return next;
}
