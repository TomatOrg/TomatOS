#include "scheduler.h"

#include <arch/intrin.h>
#include <arch/smp.h>
#include <mem/alloc.h>
#include <mem/stack.h>
#include <time/tsc.h>

#include "pcpu.h"
#include "thread.h"

typedef struct core_scheduler_context {
    // the scheduler context
    runnable_t scheduler;

    thread_t* current_thread;

    //
    // Scheduling
    //

    // the local run queue of threads to run
    _Atomic(uint32_t) run_queue_head;
    _Atomic(uint32_t) run_queue_tail;
    thread_t* run_queue[256];

    // the next thread to run, shares a time slice with
    // the currently running thread
    _Atomic(thread_t*) run_next;

    // the park key
    _Atomic(uintptr_t) park;

    //
    // Preemption
    //

    // when set to true preemption should not switch the context
    // but should set the want preemption flag instead
    uint32_t preempt_count;

    // we got an preemption request while preempt count was 0
    // next time we enable preemption make sure to preempt
    bool want_preemption;
} core_scheduler_context_t;

/**
 * The current cpu's context
 */
static CPU_LOCAL core_scheduler_context_t m_core = {};

/**
 * The scheduler contexts of all cpus
 */
static core_scheduler_context_t** m_all_cores = NULL;

err_t scheduler_init(void) {
    err_t err = NO_ERROR;

    m_all_cores = mem_alloc(g_cpu_count * sizeof(core_scheduler_context_t*));
    CHECK_ERROR(m_all_cores != NULL, ERROR_OUT_OF_MEMORY);

cleanup:
    return err;
}

err_t scheduler_init_per_core(void) {
    err_t err = NO_ERROR;

    // save the pointer of the current process
    m_all_cores[get_cpu_id()] = &m_core;

    void* stack = small_stack_alloc();
    CHECK_ERROR(stack != NULL, ERROR_OUT_OF_MEMORY);
    runnable_set_rsp(&m_core.scheduler, stack);

cleanup:
    return err;
}

thread_t* scheduler_get_current_thread(void) {
    return m_core.current_thread;
}

//----------------------------------------------------------------------------------------------------------------------
// Thread queue
//----------------------------------------------------------------------------------------------------------------------

typedef struct thread_queue {
    thread_t* head;
    thread_t* tail;
} thread_queue_t;

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Scheduler invocation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef void (*scheduler_func_t)(void);

/**
 * Performs a scheduler call, must be done with preemption disabled
 */
static void scheduler_do_call(scheduler_func_t callback) {
    runnable_set_rip(&m_core.scheduler, callback);
    runnable_switch(&m_core.current_thread->runnable, &m_core.scheduler);
}

/**
 * Perform a scheduler call, this will disable preemption between
 * the calls to make sure we won't get any weird double switch
 */
static void scheduler_call(scheduler_func_t callback) {
    m_core.preempt_count = true;
    scheduler_do_call(callback);
    m_core.preempt_count = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual scheduler
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * The global scheduler lock
 */
static spinlock_t m_scheduler_lock = INIT_SPINLOCK();

/**
 * How many cores are idle
 */
static _Atomic(int32_t) m_cores_idle = 0;

/**
 * Bitmask of cores in idle list, one bit per core.
 */
static _Atomic(uint64_t) m_idle_mask = 0;

/**
 * The global run queue of threads
 */
static thread_queue_t m_global_run_queue = {};

/**
 * The size of the run queue
 */
static uint32_t m_global_run_queue_size = 0;

//----------------------------------------------------------------------------------------------------------------------
// Core sleeping and waking up
//----------------------------------------------------------------------------------------------------------------------

static void core_stop(void) {
    spinlock_lock(&m_scheduler_lock);
    // TODO: idle list
    m_idle_mask |= 1 << get_cpu_id();
    m_cores_idle++;
    spinlock_unlock(&m_scheduler_lock);

    // and now wait until someone tells us to wakeup
    while (m_core.park == 0) {
        // set up the monitor
        __monitor((uintptr_t)&m_core.park, 0, 0);

        // ensure again that nothing changed
        if (m_core.park != 0) {
            break;
        }

        // and now wait for the memory write
        __mwait(0, 0);
    }

    // and clear it
    m_core.park = 0;
}

// static void core_wake(core_scheduler_context_t* core) {
//     // TODO: wake up the core
//     uint32_t old = atomic_exchange(&core->park, 1);
//     ASSERT(old == 0 && !"double wakeup");
// }

//----------------------------------------------------------------------------------------------------------------------
// Global run queue
//----------------------------------------------------------------------------------------------------------------------

static void core_run_queue_put(thread_t* thread, bool next);

static void global_run_queue_put(thread_queue_t* batch, uint32_t n) {
    thread_queue_push_back_all(&m_global_run_queue, batch);
    m_global_run_queue_size += n;
    *batch = (thread_queue_t){};
}

static thread_t* global_run_queue_get(uint32_t max) {
    if (m_global_run_queue_size == 0) {
        return NULL;
    }

    // limit by the global entries that would be available per core
    uint32_t n = m_global_run_queue_size / g_cpu_count + 1;
    if (n > m_global_run_queue_size) {
        n = m_global_run_queue_size;
    }

    // limit by the maximum requested
    if (max > 0 && n > max) {
        n = max;
    }

    // only fill up to half the local run queue
    if (n > ARRAY_LENGTH(m_core.run_queue) / 2) {
        n = ARRAY_LENGTH(m_core.run_queue) / 2;
    }

    m_global_run_queue_size -= n;

    // get the thread to run
    thread_t* thread = thread_queue_pop(&m_global_run_queue);

    // steal more into our local run queue while we are at it
    n--;
    for (; n > 0; n--) {
        thread_t* thread2 = thread_queue_pop(&m_global_run_queue);
        core_run_queue_put(thread2, false);
    }

    return thread;
}

//----------------------------------------------------------------------------------------------------------------------
// Local run queue management
//----------------------------------------------------------------------------------------------------------------------

static bool core_run_queue_put_slow(thread_t* thread, uint32_t h, uint32_t t) {
    thread_t* batch[ARRAY_LENGTH(m_core.run_queue) / 2 + 1];

    // First, grab a batch from local queue
    uint32_t n = (t - h) / 2;
    ASSERT(n == ARRAY_LENGTH(m_core.run_queue) / 2);

    for (int i = 0; i < n; i++) {
        batch[i] = m_core.run_queue[(h + i) % ARRAY_LENGTH(m_core.run_queue)];
    }

    if (!atomic_compare_exchange_strong_explicit(
        &m_core.run_queue_head,
        &h, h + n,
        memory_order_release, memory_order_relaxed)
    ) {
        return false;
    }

    batch[n] = thread;

    // Link the threads
    for (int i = 0; i < n; i++) {
        batch[i]->sched_link = batch[i + 1];
    }

    thread_queue_t q = {
        .head = batch[0],
        .tail = batch[n],
    };

    // Now put the batch on global queue
    spinlock_lock(&m_scheduler_lock);
    global_run_queue_put(&q, n + 1);
    spinlock_unlock(&m_scheduler_lock);
    return true;
}

static void core_run_queue_put(thread_t* thread, bool next) {
    if (next) {
        thread_t* old_next = m_core.run_next;
        for (;;) {
            if (!atomic_compare_exchange_strong(&m_core.run_next, &old_next, thread)) {
                continue;
            }

            if (old_next == NULL) {
                return;
            }

            thread = old_next;
            break;
        }
    }

    for (;;) {
        // load-acquire, synchronize with consumers
        uint32_t h = atomic_load_explicit(&m_core.run_queue_head, memory_order_acquire);
        uint32_t t = atomic_load_explicit(&m_core.run_queue_tail, memory_order_relaxed);
        if (t - h < ARRAY_LENGTH(m_core.run_queue)) {
            m_core.run_queue[t % ARRAY_LENGTH(m_core.run_queue)] = thread;

            // store-release, makes the item available for consumption
            atomic_store_explicit(&m_core.run_queue_tail, t + 1, memory_order_release);
            return;
        }

        if (core_run_queue_put_slow(thread, h, t)) {
            return;
        }

        // we can only reach here if the slow put failed, meaning
        // we have space in the queue now
    }
}

static thread_t* core_run_queue_get(bool* inherit_time) {
    // if we have a next attempt to take it, do so atomically in case
    // some other core is trying to steal our next thread, if we take
    // it then inherit the time slice
    thread_t* next = m_core.run_next;
    if (next != NULL && atomic_compare_exchange_strong(&m_core.run_next, &next, NULL)) {
        *inherit_time = true;
        return next;
    }

    for (;;) {
        // load-acquire, synchronize with other consumers
        uint32_t h = atomic_load_explicit(&m_core.run_queue_head, memory_order_acquire);
        uint32_t t = atomic_load_explicit(&m_core.run_queue_tail, memory_order_relaxed);
        if (t == h) {
            return NULL;
        }

        // attempt to move the head and take the thread
        thread_t* thread = m_core.run_queue[t % ARRAY_LENGTH(m_core.run_queue)];
        if (atomic_compare_exchange_strong_explicit(&m_core.run_queue_head, &h, h + 1, memory_order_release, memory_order_relaxed)) {
            return thread;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Scheduling itself
//----------------------------------------------------------------------------------------------------------------------

static thread_t* scheduler_find_runnable(bool* inherit_time, bool* try_wake_cpu) {
    for (;;) {
        thread_t* thread = NULL;

        // TODO: check timers

        // TODO: try schedule gc worker

        // TODO: check global runnable queue

        // TODO: wake up finalizer thread

        // local run queue
        thread = core_run_queue_get(inherit_time);
        if (thread != NULL) {
            return thread;
        }

        // global run queue
        if (m_global_run_queue_size != 0) {
            spinlock_lock(&m_scheduler_lock);
            thread = global_run_queue_get(0);
            spinlock_unlock(&m_scheduler_lock);
            if (thread != NULL) {
                return thread;
            }
        }

        // TODO: poll interrupts?

        /// TODO: spinning cores

        // We have nothing to do.

        // TODO: attempt to join GC work if any

        // TODO: poll interrupts until next timer

        // stop the core
        core_stop();
    }
}

static void scheduler_execute(thread_t* thread, bool inherit_time) {
    // update into a running state
    thread_update_status(thread, THREAD_STATUS_RUNNABLE, THREAD_STATUS_RUNNING);

    // save as the current thread to run
    m_core.current_thread = thread;

    // TODO: deadline

    // we can safely resume the thread
    // TODO: fpu context
    runnable_resume(&thread->runnable);
}

static void scheduler_schedule(void) {
    for (;;) {
        bool inherit_time = false;
        bool try_wake_cpu = false;
        thread_t* thread = scheduler_find_runnable(&inherit_time, &try_wake_cpu);

        if (try_wake_cpu) {
            // TODO: wake cpu
        }

        scheduler_execute(thread, inherit_time);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Scheduler API
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void scheduler_ready(thread_t* thread) {
    // disable preemption because we are going to touch
    // alot of core local structures
    scheduler_preempt_disable();

    // Mark as runnable and put on the run queue
    thread_update_status(thread, THREAD_STATUS_WAITING, THREAD_STATUS_RUNNABLE);
    core_run_queue_put(thread, true);

    // TODO: attempt to wakeup a core if need be

    scheduler_preempt_enable();
}

void scheduler_yield(void) {
    scheduler_call(scheduler_schedule);
}

void scheduler_start_per_core(void) {
    // enable interrupts at this point
    asm("sti");

    // set the scheduler_schedule as target
    runnable_set_rip(&m_core.scheduler, scheduler_schedule);

    // use the jump since we don't have a valid thread right now
    runnable_resume(&m_core.scheduler);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Preemption handling
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void scheduler_preempt_disable(void) {
    m_core.preempt_count++;
}

void scheduler_preempt_enable(void) {
    if (--m_core.preempt_count) {
        if (m_core.want_preemption) {
            scheduler_do_call(scheduler_schedule);
        }
    }
}
