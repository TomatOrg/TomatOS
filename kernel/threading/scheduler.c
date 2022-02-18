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

#include <sync/spinlock.h>
#include <arch/intrin.h>
#include <arch/apic.h>
#include <arch/msr.h>

#include <stdatomic.h>

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
static void thread_queue_push_back_all(thread_queue_t* q, thread_queue_t* q2) {
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

static void thread_queue_push_back(thread_queue_t* q, thread_t* thread) {
    thread->sched_link = NULL;
    if (q->tail != NULL) {
        q->tail->sched_link = thread;
    } else {
        q->head = thread;
    }
    q->tail = thread;
}

static thread_t* thread_queue_pop(thread_queue_t* q) {
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

// The global run queue
static thread_queue_t m_global_run_queue;
static int32_t m_global_run_queue_size;

// The idle cpus
static uint64_t m_idle_cpus;
static uint32_t m_idle_cpus_count;

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
static void global_run_queue_put_batch(thread_queue_t* batch, int32_t n) {
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
static void global_run_queue_put(thread_t* thread) {
    thread_queue_push_back(&m_global_run_queue, thread);
    m_global_run_queue_size++;
}

/**
 * Get a thread from the global run queue
 */
static thread_t* global_run_queue_get() {
    if (m_global_run_queue_size == 0) {
        return NULL;
    }

    // take one from the queue
    m_global_run_queue_size -= 1;
    return thread_queue_pop(&m_global_run_queue);
}

static void lock_scheduler() {
    spinlock_lock(&m_scheduler_lock);
}

static void unlock_scheduler() {
    spinlock_unlock(&m_scheduler_lock);
}

/**
 * Tried to wake a cpu for running threads
 */
static void wake_cpu() {
    if (atomic_load(&m_idle_cpus_count) == 0) {
        return;
    }

    // get an idle cpu from the queue
    lock_scheduler();
    if (m_idle_cpus == 0) {
        // no idle cpu
        unlock_scheduler();
        return;
    }
    uint8_t apic_id = __builtin_clz(m_idle_cpus);
    unlock_scheduler();

    // send an ipi to schedule threads from the global run queue
    // to the found cpu
    lapic_send_ipi(IRQ_SCHEDULE, apic_id);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Local run queue
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// The head and tail for the local run queue
static uint32_t CPU_LOCAL m_run_queue_head;
static uint32_t CPU_LOCAL m_run_queue_tail;

// the local run queue elements
static thread_t* CPU_LOCAL m_run_queue[256];

// the next thread to run
static thread_t* CPU_LOCAL m_run_next;

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
static bool run_queue_put_slow(thread_t* thread, uint32_t head, uint32_t tail, uint32_t* run_queue_head) {
    thread_t* batch[ARRAY_LEN(m_run_queue) / 2 + 1];

    // First grab a batch from the local queue
    int32_t n = tail - head;
    n = n / 2;

    for (int i = 0; i < n; i++) {
        batch[i] = m_run_queue[(head + tail) % ARRAY_LEN(m_run_queue)];
    }

    if (!atomic_compare_exchange_weak_explicit(run_queue_head, &head, head + n, memory_order_release, memory_order_relaxed)) {
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
static void run_queue_put(thread_t* thread, bool next) {
    // we want this thread to run next
    if (next) {
        thread_t** runnext = get_cpu_local_base(&m_run_next);
        thread_t* old_next = m_run_next;
        // try to change the old thread to the new one
        while (!atomic_compare_exchange_weak(runnext, &old_next, thread));

        // no thread was supposed to run next, just return
        if (old_next == NULL) {
            return;
        }

        // Kick the old next to the regular run queue, continue
        // with normal logic
        thread = old_next;
    }

    // we need the absolute addresses for these for atomic accesses
    uint32_t* run_queue_head = get_cpu_local_base(&m_run_queue_head);
    uint32_t* run_queue_tail = get_cpu_local_base(&m_run_queue_tail);
    uint32_t head, tail;

    do {
        // start with a fast path
        head = atomic_load_explicit(run_queue_head, memory_order_acquire);
        tail = m_run_queue_tail;
        if (tail - head < ARRAY_LEN(m_run_queue)) {
            m_run_queue[tail % ARRAY_LEN(m_run_queue)] = thread;
            atomic_store_explicit(run_queue_tail, tail + 1, memory_order_release);
            return;
        }

        // if we failed with fast path try the slow path
        if (run_queue_put_slow(thread, head, tail, run_queue_head)) {
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
static thread_t* run_queue_get(bool* inherit_time) {
    thread_t** run_next = get_cpu_local_base(&m_run_next);

    // If there's a run next, it's the next thread to run.
    thread_t* next = m_run_next;

    // If the run next is not NULL and the CAS fails, it could only have been stolen by another cpu,
    // because other cpus can race to set run next to NULL, but only the current cpu can set it.
    // Hence, there's no need to retry this CAS if it falls.
    if (next != NULL && atomic_compare_exchange_weak(run_next, &next, NULL)) {
        *inherit_time = true;
        return next;
    }

    uint32_t* run_queue_head = get_cpu_local_base(&m_run_queue_head);

    while (true) {
        uint32_t head = atomic_load_explicit(run_queue_head, memory_order_acquire);
        uint32_t tail = m_run_queue_tail;
        if (tail == head) {
            return NULL;
        }
        thread_t* thread = m_run_queue[head & ARRAY_LEN(m_run_queue)];
        if (atomic_compare_exchange_weak_explicit(run_queue_head, &head, head + 1, memory_order_release, memory_order_relaxed)) {
            *inherit_time = false;
            return thread;
        }
    }
}

static bool run_queue_empty() {
    // Defend against a race where
    //  1) cpu has thread in run_next but run_queue_head == run_queue_tail
    //  2) run_queue_put on cpu kicks thread to the run_queue
    //  3) run_queue_get on cpu empties run_queue_next.
    // Simply observing that run_queue_head == run_queue_tail and then observing
    // that run_next == nil does not mean the queue is empty.

    uint32_t* run_queue_head = get_cpu_local_base(&m_run_queue_head);
    uint32_t* run_queue_tail = get_cpu_local_base(&m_run_queue_tail);
    thread_t** run_next = get_cpu_local_base(&m_run_next);

    while (true) {
        uint32_t head = atomic_load(run_queue_head);
        uint32_t tail = atomic_load(run_queue_tail);
        thread_t* next = atomic_load(run_next);
        if (tail == atomic_load(run_queue_tail)) {
            return head == tail && next == NULL;
        }
    }
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
    preempt_state_t preempt = scheduler_preempt_disable();

    ASSERT((get_thread_status(thread) & ~THREAD_SUSPEND) == THREAD_STATUS_WAITING);

    // Mark as runnable
    cas_thread_state(thread, THREAD_STATUS_WAITING, THREAD_STATUS_RUNNABLE);

    // Put in the run queue
    run_queue_put(thread, true);

    scheduler_preempt_enable(preempt);
}

static bool cas_from_preempted(thread_t* thread) {
    thread_status_t old = THREAD_STATUS_PREEMPTED;
    return atomic_compare_exchange_weak(&thread->status, &old, THREAD_STATUS_WAITING);
}

static bool cas_to_suspend(thread_t* thread, thread_status_t old, thread_status_t new) {
    ASSERT(new == (old | THREAD_SUSPEND));
    return atomic_compare_exchange_weak(&thread->status, &old, new);
}

static void cas_from_suspend(thread_t* thread, thread_status_t old, thread_status_t new) {
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
                thread->preempt = false;

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

                // Request synchronous preemption.
                thread->preempt_stop = true;
                thread->preempt = true;

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

preempt_state_t scheduler_preempt_disable(void) {
    preempt_state_t state = { .priority = __readcr8() };
    __writecr8(PRIORITY_NO_PREEMPT);
    return state;
}

void scheduler_preempt_enable(preempt_state_t state) {
    __writecr8(state.priority);
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

/**
 * Execute the thread on the current cpu
 *
 * @param ctx               [IN] The context of the scheduler interrupt
 * @param thread            [IN] The thread to run
 * @param inherit_time      [IN] Should the time slice be inherited or should we make a new one
 */
static void execute(interrupt_context_t* ctx, thread_t* thread, bool inherit_time) {
    // set the current thread
    m_current_thread = thread;

    // get ready to run it
    cas_thread_state(thread, THREAD_STATUS_RUNNABLE, THREAD_STATUS_RUNNING);

    if (!inherit_time) {
        // add another tick
        m_scheduler_tick++;

        // set a new timeslice of 10 milliseconds
        lapic_set_deadline(10 * 1000);
    } else if (m_scheduler_tick == 0) {
        // this is the first tick, set an initial timeslice
        lapic_set_deadline(10 * 1000);
    }

    // set the gprs context
    restore_thread_context(thread, ctx);

    // TODO: set fpu context

    // set the tcb
    __writemsr(MSR_IA32_FS_BASE, thread->tcb);
}

//----------------------------------------------------------------------------------------------------------------------
// Scheduler itself
//----------------------------------------------------------------------------------------------------------------------

static thread_t* find_runnable(bool* inherit_time) {
    thread_t* thread = NULL;

    while (true) {
        // try the local run queue first
        thread = run_queue_get(inherit_time);
        if (thread != NULL) {
            return thread;
        }

        // try the global run queue
        if (m_global_run_queue_size != 0) {
            lock_scheduler();
            thread = global_run_queue_get();
            unlock_scheduler();
            if (thread != NULL) {
                *inherit_time = false;
                return thread;
            }
        }

        // TODO: steal work from other cpus

        // mark this cpu as idle
        lock_scheduler();
        m_idle_cpus |= 1 << get_apic_id();
        m_idle_cpus_count++;
        unlock_scheduler();

        // wait for next interrupt, we are already running from interrupt
        // context so we need to re-enable interrupts first
        _enable();
        asm ("hlt");
        _disable();

        // remove from idle cpus since we might have work to do
        lock_scheduler();
        m_idle_cpus &= 1 << get_apic_id();
        m_idle_cpus_count--;
        unlock_scheduler();
    }
}

static void schedule(interrupt_context_t* ctx) {
    thread_t* thread = NULL;
    bool inherit_time = false;

    // Check the global runnable queue once in a while to ensure fairness.
    // Otherwise, two goroutines can completely occupy the local run queue
    // by constantly respawning each other.
    if ((m_scheduler_tick % 61) == 0 && m_global_run_queue_size > 0) {
        lock_scheduler();
        thread = global_run_queue_get();
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

void scheduler_on_schedule(interrupt_context_t* ctx) {
    thread_t* current_thread = get_current_thread();
    m_current_thread = NULL;

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

void scheduler_on_yield(interrupt_context_t* ctx) {
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

void scheduler_on_park(interrupt_context_t* ctx) {
    thread_t* current_thread = get_current_thread();
    m_current_thread = NULL;

    ASSERT(__readcr8() < PRIORITY_NO_PREEMPT);

    // save the state and set the thread to runnable
    save_thread_context(current_thread, ctx);

    // put the thread into a waiting state
    cas_thread_state(current_thread, THREAD_STATUS_RUNNING, THREAD_STATUS_WAITING);

    // unlock a spinlock if needed
    if (current_thread->wait_lock != NULL) {
        spinlock_unlock(current_thread->wait_lock);
        current_thread->wait_lock = NULL;
    }

    // schedule a new thread
    schedule(ctx);
}

void scheduler_on_drop(interrupt_context_t* ctx) {
    thread_t* current_thread = get_current_thread();
    m_current_thread = NULL;

    ASSERT(__readcr8() < PRIORITY_NO_PREEMPT);

    if (current_thread != NULL) {
        TRACE("TODO: release this, something with ref counts or idk");
    }

    schedule(ctx);
}

//----------------------------------------------------------------------------------------------------------------------
// Interrupts to call the scheduler
//----------------------------------------------------------------------------------------------------------------------

void scheduler_schedule() {
    asm volatile (
        "int %0"
        :
        : "i"(IRQ_SCHEDULE)
        : "memory");
}

void scheduler_yield() {
    asm volatile (
        "int %0"
        :
        : "i"(IRQ_YIELD)
        : "memory");
}

void scheduler_park() {
    asm volatile (
        "int %0"
        :
        : "i"(IRQ_PARK)
        : "memory");
}

void scheduler_drop_current() {
    asm volatile (
        "int %0"
        :
        : "i"(IRQ_DROP)
        : "memory");
}

void scheduler_startup() {
    // set to normal running priority
    __writecr8(PRIORITY_NORMAL);

    // drop the current thread in favor of starting
    // the scheduler
    scheduler_drop_current();
}

thread_t* get_current_thread() {
    return m_current_thread;
}
