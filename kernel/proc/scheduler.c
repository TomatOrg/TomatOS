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

#include "scheduler.h"

#include "cpu_local.h"
#include "kernel.h"
#include "util/stb_ds.h"
#include "runtime/dotnet/gc/gc.h"

#include <sync/spinlock.h>
#include <arch/apic.h>
#include <arch/msr.h>

#include <stdatomic.h>
#include "arch/intrin.h"

// little helper to deal with the global run queue
typedef struct thread_queue {
    thread_t* head;
    thread_t* tail;
} thread_queue_t;

/**
 * Adds all the threads in q2 to the tail of the queue. After this q2 must
 * not be used.
 *
 * @param q     [IN] The main queue
 * @param q2    [IN] The queue to empty
 */
INTERRUPT static void thread_queue_push_back_all(thread_queue_t* q, thread_queue_t* q2) {
    if (q2->tail == NULL) {
        return;
    }

    q2->tail->sched_link = NULL;
    if (q->tail != NULL) {
        q->tail->sched_link = q2->head;
    } else {
        q->head = q2->head;
    }
    q->tail = q2->tail;
}

INTERRUPT static void thread_queue_push_back(thread_queue_t* q, thread_t* thread) {
    thread->sched_link = NULL;
    if (q->tail != NULL) {
        q->tail->sched_link = thread;
    } else {
        q->head = thread;
    }
    q->tail = thread;
}

INTERRUPT static thread_t* thread_queue_pop(thread_queue_t* q) {
    thread_t* thread = q->head;
    if (thread != NULL) {
        q->head = thread->sched_link;
        if (q->head == NULL) {
            q->tail = NULL;
        }
    }
    return thread;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Global run queue
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define RUN_QUEUE_LEN 256

// The global run queue
static thread_queue_t m_global_run_queue;
static int32_t m_global_run_queue_size;

// The idle cpus
static uint64_t m_idle_cpus;
static _Atomic(uint32_t) m_idle_cpus_count;

// spinlock to protect the scheduler internal stuff
static spinlock_t m_scheduler_lock = INIT_SPINLOCK();

/**
 * Put a batch of runnable threads on the global runnable queue.
 *
 * @remark
 * The scheduler spinlock must be help while calling this function
 *
 * @param batch [IN] The thread batch to put
 * @param n     [IN] The number of threads in the batch
 */
INTERRUPT static void global_run_queue_put_batch(thread_queue_t* batch, int32_t n) {
    thread_queue_push_back_all(&m_global_run_queue, batch);
    m_global_run_queue_size += n;
    batch->tail = NULL;
    batch->head = NULL;
}

/**
 * Put a thread on the global runnable queue.
 *
 * @remark
 * The scheduler spinlock must be help while calling this function
 *
 * @param thread [IN] The thread to put
 */
INTERRUPT static void global_run_queue_put(thread_t* thread) {
    thread_queue_push_back(&m_global_run_queue, thread);
    m_global_run_queue_size++;
}

static void run_queue_put(thread_t* thread, bool next);

/**
 * Get a thread from the global run queue
 */
INTERRUPT static thread_t* global_run_queue_get(int max) {
    if (m_global_run_queue_size == 0) {
        return NULL;
    }

    int n = m_global_run_queue_size / get_cpu_count() + 1;
    if (n > m_global_run_queue_size) {
        n = m_global_run_queue_size;
    }

    if (max > 0 && n > max) {
        n = max;
    }

    if (n > RUN_QUEUE_LEN / 2) {
        n = RUN_QUEUE_LEN / 2;
    }

    // we are going to take n items
    m_global_run_queue_size -= n;

    // take the initial one
    thread_t* thread = thread_queue_pop(&m_global_run_queue);
    n--;

    // take the rest
    for (; n > 0; n--) {
        thread_t* thread1 = thread_queue_pop(&m_global_run_queue);
        run_queue_put(thread1, false);
    }

    // return the initial one
    return thread;
}

INTERRUPT static void lock_scheduler() {
    spinlock_lock(&m_scheduler_lock);
}

INTERRUPT static void unlock_scheduler() {
    spinlock_unlock(&m_scheduler_lock);
}

/**
 * Tried to wake a cpu for running threads
 */
INTERRUPT static void wake_cpu() {
    if (atomic_load(&m_idle_cpus_count) == 0) {
        return;
    }

    // get an idle cpu from the queue
    lock_scheduler();
    uint8_t apic_id = __builtin_ffs(m_idle_cpus);
    unlock_scheduler();

    if (apic_id == 0) {
        // no cpu to wake up
        return;
    } else {
        // send an ipi to schedule threads from the global run queue
        // to the found cpu
        lapic_send_ipi(IRQ_SCHEDULE, apic_id - 1);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Local run queue
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct local_run_queue {
    _Atomic(uint32_t) head;
    _Atomic(uint32_t) tail;
    thread_t* queue[256];
    _Atomic(thread_t*) next;
} local_run_queue_t;

static local_run_queue_t* m_run_queues;

__attribute__((const))
INTERRUPT static local_run_queue_t* get_run_queue() {
    return &m_run_queues[get_cpu_id()];
}

__attribute__((const))
INTERRUPT static local_run_queue_t* get_run_queue_of(int cpu_id) {
    return &m_run_queues[cpu_id];
}

/**
 * Slow path for run queue, called when we failed to add items to the local run queue,
 * so we are going to put away some of our threads to the global run queue instead
 *
 * @param thread            [IN] The thread we want to run
 * @param head              [IN] The head when we tried to insert
 * @param tail              [IN] The tail when we tried to insert
 *
 * @param run_queue_head    [IN] Absolute address of the run_queue, already resolved
 *                               by caller so pass it to us instead of needing to recalculate it
 */
INTERRUPT static bool run_queue_put_slow(thread_t* thread, uint32_t head, uint32_t tail) {
    local_run_queue_t* rq = get_run_queue();
    thread_t* batch[RUN_QUEUE_LEN / 2 + 1];

    // First grab a batch from the local queue
    int32_t n = tail - head;
    n = n / 2;

    for (int i = 0; i < n; i++) {
        batch[i] = rq->queue[(head + i) % RUN_QUEUE_LEN];
    }

    if (!atomic_compare_exchange_weak_explicit(&rq->head, &head, head + n, memory_order_release, memory_order_relaxed)) {
        return false;
    }

    batch[n] = thread;

    // Link the threads
    for (int i = 0; i < n; i++) {
        batch[i]->sched_link = batch[i + 1];
    }

    thread_queue_t queue = {
        .head = batch[0],
        .tail = batch[n]
    };

    // now put the batch on the global queue
    lock_scheduler();
    global_run_queue_put_batch(&queue, n + 1);
    unlock_scheduler();

    return true;
}

/**
 * Tried to put a thread on the local runnable queue.
 *
 * If the local run queue is full the thread will be put to the global queue.
 *
 * @remark
 * If next is true, this will always be put in the current run queue next, kicking out
 * whatever that was in there, potentially putting it in the global run queue
 *
 * @param thread        [IN] The thread to queue
 * @param next          [IN] Should the thread be put in the head or tail of the runnable queue
 */
INTERRUPT static void run_queue_put(thread_t* thread, bool next) {
    local_run_queue_t* rq = get_run_queue();

    // we want this thread to run next
    if (next) {
        thread_t* old_next = rq->next;
        thread_t* temp = old_next;
        // try to change the old thread to the new one
        while (!atomic_compare_exchange_weak(&rq->next, &temp, thread)) {
            old_next = rq->next;
            temp = old_next;
        }

        // no thread was supposed to run next, just return
        if (old_next == NULL) {
            return;
        }

        // Kick the old next to the regular run queue, continue
        // with normal logic
        thread = old_next;
    }

    // we need the absolute addresses for these for atomic accesses
    uint32_t head, tail;

    do {
        // start with a fast path
        head = atomic_load_explicit(&rq->head, memory_order_acquire);
        tail = rq->tail;
        if (tail - head < RUN_QUEUE_LEN) {
            rq->queue[tail % RUN_QUEUE_LEN] = thread;
            atomic_store_explicit(&rq->tail, tail + 1, memory_order_release);
            return;
        }

        // if we failed with fast path try the slow path
        if (run_queue_put_slow(thread, head, tail)) {
            // we put threads on the global queue, wakeup a cpu if
            // possible to run them
            wake_cpu();
            return;
        }

        // if failed both try again
    } while (true);
}

/**
 * Get a thread from the local runnable queue.
 *
 * If inheritTime is set to true if should inherit the remaining time
 * in the current time slice. Otherwise, it should start a new time slice.
 */
INTERRUPT static thread_t* run_queue_get(bool* inherit_time) {
    local_run_queue_t* rq = get_run_queue();

    // If there's a run next, it's the next thread to run.
    thread_t* next = rq->next;

    // If the run next is not NULL and the CAS fails, it could only have been stolen by another cpu,
    // because other cpus can race to set run next to NULL, but only the current cpu can set it.
    // Hence, there's no need to retry this CAS if it falls.
    thread_t* temp_next = next;
    if (next != NULL && atomic_compare_exchange_weak(&rq->next, &temp_next, NULL)) {
        *inherit_time = true;
        return next;
    }

    while (true) {
        uint32_t head = atomic_load_explicit(&rq->head, memory_order_acquire);
        uint32_t tail = rq->tail;
        if (tail == head) {
            return NULL;
        }
        thread_t* thread = rq->queue[head % RUN_QUEUE_LEN];
        if (atomic_compare_exchange_weak_explicit(&rq->head, &head, head + 1, memory_order_release, memory_order_relaxed)) {
            *inherit_time = false;
            return thread;
        }
    }
}

INTERRUPT static bool run_queue_empty() {
    // Defend against a race where
    //  1) cpu has thread in run_next but run_queue_head == run_queue_tail
    //  2) run_queue_put on cpu kicks thread to the run_queue
    //  3) run_queue_get on cpu empties run_queue_next.
    // Simply observing that run_queue_head == run_queue_tail and then observing
    // that run_next == nil does not mean the queue is empty.
    local_run_queue_t* rq = get_run_queue();
    while (true) {
        uint32_t head = atomic_load(&rq->head);
        uint32_t tail = atomic_load(&rq->tail);
        thread_t* next = atomic_load(&rq->next);
        if (tail == atomic_load(&rq->tail)) {
            return head == tail && next == NULL;
        }
    }
}

/**
 * Grab items from the run queue of another cpu
 *
 * @param cpu_id            [IN] The other cpu to take from
 * @param steal_run_next    [IN] Should we attempt to take the next item
 */
INTERRUPT static uint32_t run_queue_grab(int cpu_id, thread_t** batch, uint32_t batchHead, bool steal_run_next) {
    local_run_queue_t* orq = get_run_queue_of(cpu_id);

    while (true) {
        // get the head and tail
        uint32_t h = atomic_load_explicit(&orq->head, memory_order_acquire);
        uint32_t t = atomic_load_explicit(&orq->tail, memory_order_acquire);

        // calculate the amount to take
        uint32_t n = t - h;
        n = n - n / 2;

        // if there is nothing to take try to take the
        // next thread to run
        if (n == 0) {
            if (steal_run_next) {
                // Try to steal from run_queue_next
                thread_t* next = orq->next;
                if (next != NULL) {
                    if (!atomic_compare_exchange_weak(&orq->next, &next, NULL)) {
                        continue;
                    }
                    batch[batchHead % RUN_QUEUE_LEN] = next;
                    return 1;
                }
            }
            return 0;
        }

        // read inconsistent h and t
        if (n > RUN_QUEUE_LEN / 2) {
            continue;
        }

        // take the items we want
        for (int i = 0; i < n; i++) {
            thread_t* thread = orq->queue[(h + i) % RUN_QUEUE_LEN];
            batch[(batchHead + i) % RUN_QUEUE_LEN] = thread;
        }

        // Try and increment the head since we taken from the queue
        if (atomic_compare_exchange_weak(&orq->head, &h, h + n)) {
            return n;
        }
    }
}

/**
 * Steal from the run queue of another cpu
 *
 * @param cpu_id            [IN] The cpu to steal from
 * @param steal_run_next    [IN] Should we steal from the next thread
 */
INTERRUPT static thread_t* run_queue_steal(int cpu_id, bool steal_run_next) {
    local_run_queue_t* rq = get_run_queue();

    uint32_t t = rq->tail;
    uint32_t n = run_queue_grab(cpu_id, rq->queue, t, steal_run_next);
    if (n == 0) {
        // we failed to grab
        return NULL;
    }
    n--;

    // take the first thread
    thread_t* thread = rq->queue[(t + n) % RUN_QUEUE_LEN];
    if (n == 0) {
        // we only took a single thread, no need to queue it
        return thread;
    }

    uint32_t h = atomic_load_explicit(&rq->head, memory_order_acquire);
    ASSERT(t - h + n < RUN_QUEUE_LEN);
    atomic_store_explicit(&rq->tail, t + n, memory_order_release);

    return thread;
}

bool scheduler_can_spin(int i) {
    // don't spin anymore...
    if (i > 4) return false;

    // single core machine, never spin
    if (get_cpu_count() <= 1) return false;

    // All cpus are doing work, so we might need to
    // do work as well
    if (m_idle_cpus_count == 0) return false;

    // we have stuff to run on our local run queue
    if (!run_queue_empty()) return false;

    // we can spin a little :)
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Wake a thread
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void scheduler_ready_thread(thread_t* thread) {
    scheduler_preempt_disable();

    ASSERT((get_thread_status(thread) & ~THREAD_SUSPEND) == THREAD_STATUS_WAITING);

    // Mark as runnable
    cas_thread_state(thread, THREAD_STATUS_WAITING, THREAD_STATUS_RUNNABLE);

    // Put in the run queue
    run_queue_put(thread, true);

    scheduler_preempt_enable();
}

INTERRUPT static bool cas_from_preempted(thread_t* thread) {
    thread_status_t old = THREAD_STATUS_PREEMPTED;
    return atomic_compare_exchange_weak(&thread->status, &old, THREAD_STATUS_WAITING);
}

INTERRUPT static bool cas_to_suspend(thread_t* thread, thread_status_t old, thread_status_t new) {
    ASSERT(new == (old | THREAD_SUSPEND));
    return atomic_compare_exchange_weak(&thread->status, &old, new);
}

INTERRUPT static void cas_from_suspend(thread_t* thread, thread_status_t old, thread_status_t new) {
    bool success = false;
    if (new == (old & ~THREAD_SUSPEND)) {
        if (atomic_compare_exchange_weak(&thread->status, &old, new)) {
            success = true;
        }
    }
    ASSERT(success);
}

suspend_state_t scheduler_suspend_thread(thread_t* thread) {
    bool stopped = false;

    for (int i = 0;; i++) {
        thread_status_t status = get_thread_status(thread);
        switch (status) {
            default:
                // the thread is already suspended, make sure of it
                ASSERT(status & THREAD_SUSPEND);
                break;

            case THREAD_STATUS_DEAD:
                // nothing to suspend.
                return (suspend_state_t){ .dead = true };

            case THREAD_STATUS_PREEMPTED:
                // We (or someone else) suspended the thread. Claim
                // ownership of it by transitioning it to
                // THREAD_STATUS_WAITING
                if (!cas_from_preempted(thread)) {
                    break;
                }

                // Clear the preemption request.
                thread->preempt_stop = false;

                // We stopped the thread, so we have to ready it later
                stopped = true;

                status = THREAD_STATUS_WAITING;

                // fallthrough

            case THREAD_STATUS_RUNNABLE:
            case THREAD_STATUS_WAITING:
                // Claim goroutine by setting suspend bit.
                // This may race with execution or readying of thread.
                // The scan bit keeps it from transition state.
                if (!cas_to_suspend(thread, status, status | THREAD_SUSPEND)) {
                    break;
                }

                // Clear the preemption request.
                thread->preempt_stop = false;

                // The thread is already at a safe-point
                // and we've now locked that in.
                return (suspend_state_t) { .thread = thread, .stopped = stopped };

            case THREAD_STATUS_RUNNING:
                // Optimization: if there is already a pending preemption request
                // (from the previous loop iteration), don't bother with the atomics.
                if (thread->preempt_stop) {
                    break;
                }

                // Temporarily block state transitions.
                if (!cas_to_suspend(thread, THREAD_STATUS_RUNNING, THREAD_STATUS_RUNNING | THREAD_SUSPEND)) {
                    break;
                }

                // Request preemption
                thread->preempt_stop = true;

                // Prepare for asynchronous preemption.
                cas_from_suspend(thread, THREAD_STATUS_RUNNING | THREAD_SUSPEND, THREAD_STATUS_RUNNING);

                // Send asynchronous preemption. We do this
                // after CASing the thread back to THREAD_STATUS_RUNNING
                // because preempt may be synchronous and we
                // don't want to catch the thread just spinning on
                // its status.
                // TODO: scheduler_preempt(thread); or something
        }

        for (int x = 0; x < 10; x++) {
            __builtin_ia32_pause();
        }
    }
}

void scheduler_resume_thread(suspend_state_t state) {
    if (state.dead) {
        return;
    }

    // switch back to non-suspend state
    thread_status_t status = get_thread_status(state.thread);
    cas_from_suspend(state.thread, status, status & ~THREAD_SUSPEND);

    if (state.stopped) {
        // We stopped it, so we need to re-schedule it.
        scheduler_ready_thread(state.thread);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Preemption
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static size_t CPU_LOCAL m_preempt_disable_depth;

void scheduler_preempt_disable(void) {
    if (m_preempt_disable_depth++ == 0) {
        __writecr8(PRIORITY_NO_PREEMPT);
    }
}

void scheduler_preempt_enable(void) {
    if (--m_preempt_disable_depth == 0) {
        __writecr8(PRIORITY_NORMAL);
    }
}

bool scheduler_is_preemption(void) {
    return m_preempt_disable_depth == 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual scheduling
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * The currently running thread on the thread
 */
static thread_t* CPU_LOCAL m_current_thread;

/**
 * Ticks of the scheduler
 */
static int32_t CPU_LOCAL m_scheduler_tick;

//----------------------------------------------------------------------------------------------------------------------
// Actually running a thread
//----------------------------------------------------------------------------------------------------------------------

static void scheduler_set_deadline() {
    lapic_set_deadline(10 * 1000);
}

/**
 * Execute the thread on the current cpu
 *
 * @param ctx               [IN] The context of the scheduler interrupt
 * @param thread            [IN] The thread to run
 * @param inherit_time      [IN] Should the time slice be inherited or should we make a new one
 */
INTERRUPT static void execute(interrupt_context_t* ctx, thread_t* thread, bool inherit_time) {
    // set the current thread
    m_current_thread = thread;

    // get ready to run it
    cas_thread_state(thread, THREAD_STATUS_RUNNABLE, THREAD_STATUS_RUNNING);

    if (!inherit_time) {
        // add another tick
        m_scheduler_tick++;

        // set a new timeslice of 10 milliseconds
        scheduler_set_deadline();
    } else if (m_scheduler_tick == 0) {
        // this is the first tick, set an initial timeslice
        scheduler_set_deadline();
    }

    // set the gprs context
    restore_thread_context(thread, ctx);
}

//----------------------------------------------------------------------------------------------------------------------
// random order for randomizing work stealing
//----------------------------------------------------------------------------------------------------------------------

typedef struct random_enum {
    uint32_t i;
    uint32_t count;
    uint32_t pos;
    uint32_t inc;
} random_enum_t;

static uint32_t m_random_order_count = 0;
static uint32_t* m_random_order_coprimes = NULL;

INTERRUPT static uint32_t gcd(uint32_t a, uint32_t b) {
    while (b != 0) {
        uint32_t t = b;
        b = a % b;
        a = t;
    }
    return a;
}

INTERRUPT static void random_order_init(int count) {
    m_random_order_count = count;
    for (int i = 0; i <= count; i++) {
        if (gcd(i, count) == 1) {
            arrpush(m_random_order_coprimes, i);
        }
    }
}

INTERRUPT static random_enum_t random_order_start(uint32_t i) {
    return (random_enum_t) {
        .count = m_random_order_count,
        .pos = i % m_random_order_count,
        .inc = m_random_order_coprimes[i % arrlen(m_random_order_coprimes)]
    };
}

INTERRUPT static bool random_enum_done(random_enum_t* e) {
    return e->i == e->count;
}

INTERRUPT static void random_enum_next(random_enum_t* e) {
    e->i++;
    e->pos = (e->pos + e->inc) % e->count;
}

INTERRUPT static uint32_t random_enum_position(random_enum_t* e) {
    return e->pos;
}

static uint64_t CPU_LOCAL m_fast_rand;

INTERRUPT __uint128_t mul64(uint64_t a, uint64_t b) {
    return (__uint128_t)a * b;
}

/**
 * Implements wyrand
 */
INTERRUPT uint32_t fastrandom() {
    m_fast_rand += 0xa0761d6478bd642f;
    __uint128_t i = mul64(m_fast_rand, m_fast_rand ^ 0xe7037ed1a0b428db);
    uint64_t hi = (uint64_t)(i >> 64);
    uint64_t lo = (uint64_t)i;
    return (uint32_t)(hi ^ lo);
}

//----------------------------------------------------------------------------------------------------------------------
// Scheduler itself
//----------------------------------------------------------------------------------------------------------------------

/**
 * CPU is out of work nad is actively looking for work
 */
static bool CPU_LOCAL m_spinning;

/**
 * Number of spinning CPUs in the system
 */
static _Atomic(uint32_t) m_number_spinning = 0;

INTERRUPT static thread_t* steal_work() {
    for (int i = 0; i < 4; i++) {
        // on the last round try to steal next
        bool steal_next = i == 4 - 1;

        for (random_enum_t e = random_order_start(fastrandom()); !random_enum_done(&e); random_enum_next(&e)) {
            // get the cpu to steal from
            int cpu = random_enum_position(&e);
            if (cpu == get_cpu_id()) {
                continue;
            }

            // Don't bother to attempt to steal if the cpu is in sleep
            if (m_idle_cpus & cpu) {
                continue;
            }

            thread_t* thread = run_queue_steal(cpu, steal_next);
            if (thread != NULL) {
                return thread;
            }
        }
    }

    return NULL;
}

INTERRUPT static thread_t* find_runnable(bool* inherit_time) {
    thread_t* thread = NULL;

    while (true) {
        // try the local run queue first
        thread = run_queue_get(inherit_time);
        if (thread != NULL) {
            m_spinning = false;
            return thread;
        }

        // try the global run queue
        if (m_global_run_queue_size != 0) {
            lock_scheduler();
            thread = global_run_queue_get(0);
            unlock_scheduler();
            if (thread != NULL) {
                *inherit_time = false;
                m_spinning = false;
                return thread;
            }
        }

        //
        // Steal work from other cpus.
        //
        // limit the number of spinning cpus to half the number of busy cpus.
        // This is necessary to prevent excessive CPU consumption when
        // cpu count > 1 but the kernel parallelism is low.
        //
        if (m_spinning || 2 * atomic_load(&m_number_spinning) < get_cpu_count() - atomic_load(&m_idle_cpus_count)) {
            if (!m_spinning) {
                m_spinning = true;
                atomic_fetch_add(&m_number_spinning, 1);
            }

            // try to steal some work
            thread = steal_work();
            if (thread != NULL) {
                *inherit_time = false;
                return thread;
            }
        }

        // prepare to enter idle
        lock_scheduler();

        // try to get global work one last time
        if (m_global_run_queue_size != 0) {
            thread = global_run_queue_get(0);
            unlock_scheduler();
            *inherit_time = false;
            return thread;
        }

        // we are now idle
        m_idle_cpus |= 1 << get_cpu_id();
        m_idle_cpus_count++;

        unlock_scheduler();

        // restore the spinning since we are no longer spinning
        bool was_spinning = m_spinning;
        if (m_spinning) {
            m_spinning = false;

            if (atomic_fetch_add(&m_number_spinning, -1) == 0) {
                ASSERT(!"negative spinning");
            }
        }

        // set a quick preemption timer so we can steal work later
        lapic_set_deadline(1000000);

        // wait for next interrupt, we are already running from interrupt
        // context so we need to re-enable interrupts first
        __asm__ ("sti; hlt; cli");

        // remove from idle cpus since we might have work to do
        lock_scheduler();
        m_idle_cpus &= ~(1 << get_cpu_id());
        m_idle_cpus_count--;
        unlock_scheduler();
    }
}

INTERRUPT static void schedule(interrupt_context_t* ctx) {
    thread_t* thread = NULL;
    bool inherit_time = false;

    // Check the global runnable queue once in a while to ensure fairness.
    // Otherwise, two goroutines can completely occupy the local run queue
    // by constantly respawning each other.
    if ((m_scheduler_tick % 61) == 0 && m_global_run_queue_size > 0) {
        lock_scheduler();
        thread = global_run_queue_get(1);
        unlock_scheduler();
    }

    if (thread == NULL) {
        // get from the local run queue
        thread = run_queue_get(&inherit_time);
    }

    if (thread == NULL) {
        // will block until a thread is ready, essentially an idle loop,
        // this must return something eventually.
        thread = find_runnable(&inherit_time);
    }

    // actually run the new thread
    execute(ctx, thread, inherit_time);
}

//----------------------------------------------------------------------------------------------------------------------
// Scheduler callbacks
//----------------------------------------------------------------------------------------------------------------------

INTERRUPT void scheduler_on_schedule(interrupt_context_t* ctx, bool from_preempt) {
    thread_t* current_thread = get_current_thread();
    m_current_thread = NULL;

    // check if we are sleeping and this was just a quick wakeup
    // for the cpu to check for work stealing or gc work
    if (from_preempt && current_thread == NULL) {
        return;
    }

    ASSERT(__readcr8() < PRIORITY_NO_PREEMPT);

    // save the state and set the thread to runnable
    save_thread_context(current_thread, ctx);

    // put the thread on the global run queue
    if (current_thread->preempt_stop) {
        // set as preempted, don't add back to queue
        cas_thread_state(current_thread, THREAD_STATUS_RUNNING, THREAD_STATUS_PREEMPTED);

    } else {
        // set the thread to be runnable
        cas_thread_state(current_thread, THREAD_STATUS_RUNNING, THREAD_STATUS_RUNNABLE);

        // put in the global run queue
        lock_scheduler();
        global_run_queue_put(current_thread);
        unlock_scheduler();
    }

    // now schedule a new thread
    schedule(ctx);
}

INTERRUPT void scheduler_on_yield(interrupt_context_t* ctx) {
    thread_t* current_thread = get_current_thread();
    m_current_thread = NULL;

    ASSERT(__readcr8() < PRIORITY_NO_PREEMPT);

    // save the state and set the thread to runnable
    save_thread_context(current_thread, ctx);
    cas_thread_state(current_thread, THREAD_STATUS_RUNNING, THREAD_STATUS_RUNNABLE);

    // put the thread on the local run queue
    run_queue_put(current_thread, false);

    // schedule a new thread
    schedule(ctx);
}

INTERRUPT void scheduler_on_park(interrupt_context_t* ctx) {
    thread_t* current_thread = get_current_thread();
    m_current_thread = NULL;

    ASSERT(__readcr8() < PRIORITY_NO_PREEMPT);

    // save the state and set the thread to runnable
    save_thread_context(current_thread, ctx);

    // put the thread into a waiting state
    cas_thread_state(current_thread, THREAD_STATUS_RUNNING, THREAD_STATUS_WAITING);

    // unlock a spinlock if needed
    if (current_thread->wait_lock != NULL) {
        // this is a bit of a special case, because we are unlocking the lock
        // on behalf of the thread, we need to store its interrupt lock status
        // aside so the thread will have interrupts again
        current_thread->save_state.rflags |= current_thread->wait_lock->status ? BIT9 : 0;
        current_thread->wait_lock->status = false;

        // unlock it properly now
        spinlock_unlock(current_thread->wait_lock);
        current_thread->wait_lock = NULL;
    }

    // schedule a new thread
    schedule(ctx);
}

INTERRUPT void scheduler_on_drop(interrupt_context_t* ctx) {
    thread_t* current_thread = get_current_thread();
    m_current_thread = NULL;

    ASSERT(__readcr8() < PRIORITY_NO_PREEMPT);

    if (current_thread != NULL) {
        free_thread(current_thread);
    }

    schedule(ctx);
}

//----------------------------------------------------------------------------------------------------------------------
// Interrupts to call the scheduler
//----------------------------------------------------------------------------------------------------------------------

void scheduler_schedule() {
    __asm__ volatile (
        "int %0"
        :
        : "i"(IRQ_SCHEDULE)
        : "memory");
}

void scheduler_yield() {
    __asm__ volatile (
        "int %0"
        :
        : "i"(IRQ_YIELD)
        : "memory");
}

void scheduler_park() {
    __asm__ volatile (
        "int %0"
        :
        : "i"(IRQ_PARK)
        : "memory");
}

void scheduler_drop_current() {
    __asm__ volatile (
        "int %0"
        :
        : "i"(IRQ_DROP)
        : "memory");
}

void scheduler_startup() {
    // set to normal running priority
    __writecr8(PRIORITY_NORMAL);

    // enable interrupts
    _enable();

    // drop the current thread in favor of starting
    // the scheduler
    scheduler_drop_current();
}

INTERRUPT thread_t* get_current_thread() {
    return m_current_thread;
}

err_t init_scheduler() {
    err_t err = NO_ERROR;

    // initialize our random for the amount of cores we have
    random_order_init(get_cpu_count());

    m_run_queues = malloc(get_cpu_count() * sizeof(local_run_queue_t));
    CHECK(m_run_queues != NULL);

    cleanup:
    return err;
}
