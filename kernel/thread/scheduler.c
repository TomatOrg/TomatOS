#include "scheduler.h"

#include <arch/intrin.h>
#include <arch/smp.h>
#include <mem/alloc.h>
#include <mem/stack.h>
#include <time/tsc.h>

#include "pcpu.h"
#include "thread.h"
#include <stdnoreturn.h>

typedef struct core_scheduler_context {
    // the park location, scheduler will sleep on this
    // location and wakeup on modifications to it
    __attribute__((aligned(128)))
    _Atomic(size_t) park;

    // The scheduler's runnable
    runnable_t scheduler;

    // when set to true preemption should not switch the context
    // but should set the want preemption flag instead
    uint32_t preempt_count;

    // we got an preemption request while preempt count was 0
    // next time we enable preemption make sure to preempt
    bool want_preemption;

    // the eevdf queue of the core
    eevdf_queue_t queue;

    // lock to protect the queue
    spinlock_t queue_lock;

    // the last time we had a schedule
    uint64_t last_schedule;
} core_scheduler_context_t;

/**
 * The current cpu's context
 */
static CPU_LOCAL core_scheduler_context_t m_core = {};

/**
 * The scheduler contexts of all cpus
 */
static core_scheduler_context_t** m_all_cores = NULL;

/**
 * Mask of cores that are in idle currently
 */
static _Atomic(uint64_t) m_idle_cores = 0;

/**
 * Sum of all the weights in the system
 */
static _Atomic(uint64_t) m_total_weights = 0;

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
    return containerof(m_core.queue.current, thread_t, scheduler_node);
}

static uint32_t scheduler_get_ideal_weight(void) {
    return atomic_load_explicit(&m_total_weights, memory_order_relaxed) / g_cpu_count;
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
    runnable_switch(&scheduler_get_current_thread()->runnable, &m_core.scheduler);
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
// Core sleeping and waking up
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void core_wait(void) {
    // set up the monitor
    __monitor((uintptr_t)&m_core.park, 0, 0);

    // ensure again that nothing changed
    if (atomic_load_explicit(&m_core.park, memory_order_acquire) != 0) {
        return;
    }

    // and now wait for the memory write
    __mwait(0, 0);
}

static void core_prepare_park(void) {
    atomic_store_explicit(&m_core.park, 1, memory_order_relaxed);
}

static bool core_timed_out(void) {
    return atomic_load_explicit(&m_core.park, memory_order_relaxed) != 0;
}

static void core_park(void) {
    // start by disabling the deadline so we
    // won't have a spurious wakeup
    tsc_set_deadline(0);

    // and now wait until someone tells us to wakeup
    while (atomic_load_explicit(&m_core.park, memory_order_acquire) != 0) {
        core_wait();
    }
}

static bool core_park_until(uint64_t deadline) {
    // start by setting the deadline
    tsc_set_deadline(deadline);

    // as long as no one tells us to wakeup, wait
    while (atomic_load_explicit(&m_core.park, memory_order_acquire) != 0) {
        // check that we did not get a timeout in the meantime
        uint64_t now = tsc_get_usecs();
        if (deadline <= now) {
            return false;
        }

        // and now wait
        core_wait();
    }

    return true;
}

static void core_unpark(core_scheduler_context_t* core) {
    atomic_store_explicit(&m_core.park, 0, memory_order_release);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual scheduler
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

noreturn static void scheduler_execute(thread_t* thread) {
    // TODO: if not running a task restore fpu context

    // set the time slice
    tsc_set_deadline(tsc_get_usecs() + thread->scheduler_node.time_slice);

    // we can safely resume the thread
    runnable_resume(&thread->runnable);
}

static void scheduler_schedule(bool remove, bool requeue) {
    uint64_t current = tsc_get_usecs();
    int64_t time_slice = (int64_t)(current - m_core.last_schedule);
    m_core.last_schedule = current;

    for (;;) {
        // choose the next thread to run
        eevdf_node_t* choosen = eevdf_queue_schedule(&m_core.queue, time_slice, remove, requeue);

        if (choosen == NULL) {
            // TODO: attempt to steal from another core that is busy
        }

        if (choosen != NULL) {
            thread_t* thread = containerof(choosen, thread_t, scheduler_node);
            scheduler_execute(thread);
        }

        // prepare the park
        core_prepare_park();

        // reset the scheduling stuff, since we will not have a current now
        time_slice = 0;
        remove = false;
        requeue = false;

        // mark as idle
        atomic_fetch_or_explicit(&m_idle_cores, 1ull << get_cpu_id(), memory_order_relaxed);

        // could not find one, put the core to sleep
        // and wait until there is something available
        core_park();

        // mark as not idle
        atomic_fetch_and_explicit(&m_idle_cores, ~(1ull << get_cpu_id()), memory_order_relaxed);
    }
}

static void scheduler_yield_internal(void) {
    // requeue the current thread
    scheduler_schedule(false, true);
}

static void scheduler_park_internal(void) {
    // don't requeue the current thread
    scheduler_schedule(false, false);
}

static void scheduler_exit_internal(void) {
    // free the thread
    thread_free(scheduler_get_current_thread());

    // and schedule
    scheduler_schedule(true, false);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Scheduler API
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Get the scheduler context of a parking thread
 */
static core_scheduler_context_t* get_thread_scheduler_context(thread_t* thread) {
    return containerof(thread->scheduler_node.queue, core_scheduler_context_t, queue);
}

void scheduler_start_thread(thread_t* thread) {
    scheduler_preempt_disable();
    spinlock_lock(&m_core.queue_lock);

    // TODO: check if we should maybe push this to another core

    // just add the thread to the current queue
    eevdf_queue_add(&m_core.queue, &thread->scheduler_node);

    spinlock_unlock(&m_core.queue_lock);
    scheduler_preempt_enable();
}

void scheduler_wakeup_thread(thread_t* thread) {
    core_scheduler_context_t* ctx = get_thread_scheduler_context(thread);

    scheduler_preempt_disable();
    spinlock_lock(&ctx->queue_lock);

    // and call the queue wakeup
    eevdf_queue_wakeup(&ctx->queue, &thread->scheduler_node);

    // force unpark the core
    core_unpark(ctx);

    spinlock_unlock(&ctx->queue_lock);
    scheduler_preempt_enable();
}

void scheduler_yield(void) {
    scheduler_call(scheduler_yield_internal);
}

void scheduler_park(void) {
    scheduler_call(scheduler_park_internal);
}

void scheduler_exit(void) {
    scheduler_call(scheduler_exit_internal);
}

void scheduler_start_per_core(void) {
    // enable interrupts at this point
    asm("sti");

    // set the scheduler_schedule as target
    runnable_set_rip(&m_core.scheduler, scheduler_yield_internal);

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
    if (m_core.preempt_count == 1 && m_core.want_preemption) {
        scheduler_do_call(scheduler_yield_internal);
    }

    --m_core.preempt_count;
}
