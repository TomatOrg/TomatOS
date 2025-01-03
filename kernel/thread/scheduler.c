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
    int64_t preempt_count;

    // we got an preemption request while preempt count was 0
    // next time we enable preemption make sure to preempt
    bool want_preemption;

    // the eevdf queue of the core
    list_t queue;

    // lock to protect the queue
    spinlock_t queue_lock;

    // the current thread
    thread_t* current;
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

    // setup the task for the scheduler
    void* stack = small_stack_alloc();
    CHECK_ERROR(stack != NULL, ERROR_OUT_OF_MEMORY);
    runnable_set_rsp(&m_core.scheduler, stack);

    // start with a preempt count of 1, because we go to the scheduler right away
    m_core.preempt_count = 1;

    // and init the queue
    list_init(&m_core.queue);

cleanup:
    return err;
}

thread_t* scheduler_get_current_thread(void) {
    return m_core.current;
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
    m_core.preempt_count++;
    scheduler_do_call(callback);
    m_core.preempt_count--;
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
    tsc_disable_timeout();

    // and now wait until someone tells us to wakeup
    while (atomic_load_explicit(&m_core.park, memory_order_acquire) != 0) {
        core_wait();
    }
}

static bool core_park_until(uint64_t timeout) {
    // start by setting the deadline
    uint64_t deadline = tsc_set_timeout(timeout);

    // as long as no one tells us to wakeup, wait
    while (atomic_load_explicit(&m_core.park, memory_order_acquire) != 0) {
        // check that we did not get a timeout in the meantime
        if (deadline <= get_tsc()) {
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

    // set the current thread
    m_core.current = thread;

    // set the time slice
    tsc_set_timeout(ms_to_tsc(10));

    // we can safely resume the thread
    runnable_resume(&thread->runnable);
}

/**
 * Perform the scheduling, this function must be called on the scheduler stack
 * and must be called with preemption disabled and interrupts enabled
 */
static void scheduler_schedule(bool remove, bool requeue) {
    ASSERT(m_core.preempt_count != 0);

    for (;;) {
        // choose the next thread to run, we need to disable interrupts to make sure
        // that interrupts don't attempt to wake up any thread
        asm("cli");
        spinlock_lock(&m_core.queue_lock);
        if (requeue && m_core.current != NULL) {
            list_add_tail(&m_core.queue, &m_core.current->scheduler_node);
        }
        list_entry_t* choosen = list_pop(&m_core.queue);
        spinlock_unlock(&m_core.queue_lock);
        asm("sti");

        // if we did not find anything, attempt to steal
        if (choosen == NULL) {
            // TODO: this
        }

        // found something to run, so run it
        if (choosen != NULL) {
            thread_t* thread = containerof(choosen, thread_t, scheduler_node);
            scheduler_execute(thread);
        }

        // prepare the park
        core_prepare_park();

        // reset the scheduling stuff, since we will not have a current now
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

void scheduler_start_thread(thread_t* thread) {
    scheduler_preempt_disable();
    spinlock_lock(&m_core.queue_lock);

    // TODO: check if we should maybe push this to another core

    // just add the thread to the current queue
    list_add(&m_core.queue, &thread->scheduler_node);

    spinlock_unlock(&m_core.queue_lock);
    scheduler_preempt_enable();
}

void scheduler_wakeup_thread(thread_t* thread) {
    // TODO: wake on the core that it was already on

    scheduler_preempt_disable();
    spinlock_lock(&m_core.queue_lock);

    // and call the queue wakeup
    list_add(&m_core.queue, &thread->scheduler_node);

    spinlock_unlock(&m_core.queue_lock);
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

void scheduler_preempt(void) {
    // if we have a preempt count then don't call
    // the yield
    if (m_core.preempt_count != 0) {
        m_core.want_preemption = true;
        return;
    }

    ASSERT(scheduler_get_current_thread() != NULL);

    // we can safely call the yield, this will ensure
    // interrupts are enabled
    m_core.preempt_count++;
    runnable_set_rip(&m_core.scheduler, scheduler_yield_internal);
    runnable_switch_enable_interrupts(&scheduler_get_current_thread()->runnable, &m_core.scheduler);
    m_core.preempt_count--;
}

void scheduler_start_per_core(void) {
    // make sure the preempt count is non-zero
    m_core.preempt_count++;

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
    ASSERT(m_core.preempt_count >= 0);
}
