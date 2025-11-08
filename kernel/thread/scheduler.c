#include "scheduler.h"

#include <cpuid.h>
#include <arch/intrin.h>
#include <arch/smp.h>
#include <mem/alloc.h>
#include <mem/stack.h>
#include <time/tsc.h>

#include "pcpu.h"
#include "thread.h"
#include <stdnoreturn.h>

#include "mem/phys.h"
#include "time/timer.h"

typedef struct core_parker {
    atomic_bool parked;
} core_parker_t;

typedef struct core_scheduler_context {
    // the core parker of that thread
    core_parker_t* core_parker;

    // The scheduler's runnable
    void* scheduler_stack;

    // the eevdf queue of the core
    list_t queue;

    // lock to protect the queue
    irq_spinlock_t queue_lock;

    // the current thread
    thread_t* current;

    // the callback to run when we are parking
    scheduler_park_callback_t park_callback;
    void* park_arg;

    // the timer used for scheduling
    timer_t timer;

    // when set to true preemption should not switch the context
    // but should set the want preemption flag instead
    int64_t preempt_count;

    // we got an preemption request while preempt count was 0
    // next time we enable preemption make sure to preempt
    bool want_reschedule;
} core_scheduler_context_t;

/**
 * The current cpu's context
 */
static CPU_LOCAL core_scheduler_context_t m_core = {};

/**
 * The size of the core parker, for mwait to work properly
 */
static size_t m_core_parker_size = 0;

err_t init_scheduler(void) {
    err_t err = NO_ERROR;

    // calculate the monitor size so we can properly setup the wakeup structures
    uint32_t a, b, c, d;
    __cpuid(0x5, a, b, c, d);
    uint32_t min_monitor_size = (uint16_t)a;
    uint32_t max_monitor_size = (uint16_t)b;

    // check that the structure fits within the minimum
    if (min_monitor_size != 0) {
        CHECK(sizeof(core_parker_t) <= min_monitor_size);
    }

    // have a fallback incase that the cpuid did not return a valid value
    // for the max monitor size
    if (max_monitor_size == 0) {
        WARN("scheduler: mwait granularity unknown! falling back on 128");
        max_monitor_size = 128;
    }
    CHECK(sizeof(core_parker_t) <= max_monitor_size);

    // allocate all of the core parkers and setup their size
    m_core_parker_size = ALIGN_UP(sizeof(core_parker_t), max_monitor_size);

cleanup:
    return err;
}

err_t scheduler_init_per_core(void) {
    err_t err = NO_ERROR;

    // setup the stack for the scheduler
    m_core.scheduler_stack = small_stack_alloc();
    CHECK_ERROR(m_core.scheduler_stack != NULL, ERROR_OUT_OF_MEMORY);

    // start with a preempt count of 1, because we go to the scheduler right away
    m_core.preempt_count = 1;

    // setup the core parker structure
    m_core.core_parker = phys_alloc(m_core_parker_size);
    CHECK(m_core.core_parker != NULL);

    // and init the queue
    list_init(pcpu_get_pointer(&m_core.queue));

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

void switch_to_scheduler(thread_frame_t** frame, void* function, void* stack);
void jump_to_scheduler(void* function, void* stack);

/**
 * Performs a scheduler call, must be done with preemption disabled
 *
 * This will always return with preemption enabled, so no need to enable preemption
 * manually after calling this function
 */
static void scheduler_do_call(scheduler_func_t callback) {
    switch_to_scheduler(&m_core.current->cpu_state, callback, m_core.scheduler_stack);
}

/**
 * Perform a scheduler call, this will disable preemption between
 * the calls to make sure we won't get any weird double switch
 */
static void scheduler_call(scheduler_func_t callback) {
    scheduler_preempt_disable();
    scheduler_do_call(callback);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Core sleeping and waking up
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void core_prepare_park() {
    atomic_store_explicit(&m_core.core_parker->parked, true, memory_order_relaxed);
}

static void core_wait() {
    // TODO: we should have some way to setup a timeout to enable deeper sleep states after some time
    __monitor((uintptr_t)&m_core.core_parker->parked, 0, 0);
    if (atomic_load_explicit(&m_core.core_parker->parked, memory_order_acquire)) {
        __mwait(0, 0);
    }
}

static void core_park() {
    while (atomic_load_explicit(&m_core.core_parker->parked, memory_order_acquire)) {
        core_wait();
    }
}

static void core_unpark(core_parker_t* parker) {
    atomic_store_explicit(&parker->parked, false, memory_order_release);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual scheduler
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * This is called when the timer fires,
 * just ask for a reschedule
 */
static void scheduler_timer_tick(timer_t* timer) {
    scheduler_reschedule();
}

static void scheduler_drop_thread() {
    thread_save_extended_state(m_core.current);
    m_core.current = NULL;
}

noreturn static void scheduler_execute(thread_t* thread, bool inherit_time) {
    // the scheduler itself runs with interrupts enabled, but
    // for this part we want to ensure that no interrupt (and
    // especially timers) can jump in and ruin our fun
    irq_disable();

    // mark that we don't want to reschedule, since we
    // just executed something
    m_core.want_reschedule = false;

    // zero out the preemption count, so that the thread
    // can preempt
    m_core.preempt_count = 0;

    if (inherit_time) {
        // use the current deadline, if its in the past we will just
        // get the timer interrupt and handle it properly outside
        timer_set(pcpu_get_pointer(&m_core.timer), scheduler_timer_tick, m_core.timer.deadline);
    } else {
        // set the timeslice for the thread
        // TODO: have the scheduler decide on a timeslice dynamically
        timer_set(pcpu_get_pointer(&m_core.timer), scheduler_timer_tick, tsc_ms_deadline(10));
    }

    // set ourselves as the currently running thread
    m_core.current = thread;

    // set as running
    thread_switch_status(thread, THREAD_STATUS_RUNNABLE, THREAD_STATUS_RUNNING);

    // and jump back into the thread, this will also properly
    // enable interrupts for the thread
    thread_resume(thread);
}

noreturn static void scheduler_schedule(void) {
    core_scheduler_context_t* core = pcpu_get_pointer(&m_core);

    // cancel the schedule timer, since we don't need it anymore
    // and we don't want it to cause a wakeup
    timer_cancel(&core->timer);

    for (;;) {
        // prepare to park, we are going to wait for
        // a thread inside to ensure that we don't
        // get a new thread in the middle
        core_prepare_park();

        // take an item from the queue (if any)
        bool irq_state = irq_spinlock_acquire(&core->queue_lock);
        list_entry_t* next = list_pop(&core->queue);
        irq_spinlock_release(&core->queue_lock, irq_state);

        // we have a thread to run!
        if (next != NULL) {
            // ensure we are not marked as parked anymore
            core_unpark(m_core.core_parker);

            // get the thread and execute it
            thread_t* thread = containerof(next, thread_t, scheduler_node);
            scheduler_execute(thread, false);
        }

        // just park until we either get an interrupt
        // or something wakes us up, if we got a new thread
        // in between this will not actually sleep and just
        // return so we can try again
        core_park();
    }
}

static void scheduler_yield_internal(void) {
    ASSERT(m_core.preempt_count == 1);
    ASSERT(is_irq_enabled());

    thread_t* current = m_core.current;

    // switch to be runnable instead of running
    thread_switch_status(current, THREAD_STATUS_RUNNING, THREAD_STATUS_RUNNABLE);

    // drop the thread
    scheduler_drop_thread();

    // return it to the queue
    core_scheduler_context_t* core = pcpu_get_pointer(&m_core);
    bool irq_state = irq_spinlock_acquire(&core->queue_lock);
    list_add_tail(&core->queue, &current->scheduler_node);
    irq_spinlock_release(&core->queue_lock, irq_state);

    // call the scheduler
    scheduler_schedule();
}

static void scheduler_park_internal(void) {
    ASSERT(m_core.preempt_count == 1);
    ASSERT(is_irq_enabled());

    // Mark the thread as waiting now
    thread_t* current = m_core.current;
    thread_switch_status(m_core.current, THREAD_STATUS_RUNNING, THREAD_STATUS_WAITING);

    // Drop it, since we don't need it anymore
    scheduler_drop_thread();

    // run the parking callback, if it returns false then we have a failure
    // and we should let the thread run again
    if (m_core.park_callback != NULL) {
        bool ok = m_core.park_callback(m_core.park_arg);
        m_core.park_callback = NULL;
        m_core.park_arg = NULL;
        if (!ok) {
            thread_switch_status(current, THREAD_STATUS_WAITING, THREAD_STATUS_RUNNABLE);
            scheduler_execute(current, true);
        }
    }

    // let the scheduler cook
    scheduler_schedule();
}

static void scheduler_exit_internal(void) {
    ASSERT(m_core.preempt_count == 1);
    ASSERT(is_irq_enabled());

    thread_t* thread = m_core.current;

    // mark the thread as dead
    thread_switch_status(thread, THREAD_STATUS_RUNNING, THREAD_STATUS_DEAD);

    // no longer the current, and no need to save the
    // thread state since we are exiting
    m_core.current = NULL;

    // and we can free the object
    thread_free(thread);

    // schedule a new thread
    scheduler_schedule();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Scheduler API
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void scheduler_wakeup_thread(thread_t* thread) {
    scheduler_preempt_disable();

    // Mark runnable
    thread_switch_status(thread, THREAD_STATUS_WAITING, THREAD_STATUS_RUNNABLE);

    // queue it properly
    core_scheduler_context_t* core = pcpu_get_pointer(&m_core);
    bool irq_state = irq_spinlock_acquire(&core->queue_lock);
    list_add(&core->queue, &thread->scheduler_node);
    irq_spinlock_release(&core->queue_lock, irq_state);

    // perform a reschedule, to allow the new thread to run
    scheduler_reschedule();

    scheduler_preempt_enable();
}

void scheduler_yield(void) {
    ASSERT(m_core.preempt_count == 0);
    scheduler_call(scheduler_yield_internal);
}

void scheduler_park(scheduler_park_callback_t callback, void* arg) {
    ASSERT(m_core.preempt_count == 0);
    ASSERT(m_core.current->status == THREAD_STATUS_RUNNING);

    scheduler_preempt_disable();
    m_core.park_arg = arg;
    m_core.park_callback = callback;
    scheduler_do_call(scheduler_park_internal);
}

void scheduler_exit(void) {
    ASSERT(m_core.preempt_count == 0);
    scheduler_call(scheduler_exit_internal);
}

void scheduler_reschedule(void) {
    // check if we can even reschedule
    if (m_core.preempt_count != 0) {
        // mark that we need to reschedule
        m_core.want_reschedule = true;

        // wakeup the core, so it will exit
        // from the sleep loop
        if (m_core.core_parker->parked) {
            core_unpark(m_core.core_parker);
        }

        return;
    }

    // ensure we have a current thread
    ASSERT(m_core.current != NULL);

    // we can safely call yield
    scheduler_call(scheduler_yield_internal);
}

void scheduler_start_per_core(void) {
    // we should have a non-zero preempt count in here
    ASSERT(m_core.preempt_count == 1);

    // jump into the scheduler and schedule, we don't need the
    // context we were at anymore
    jump_to_scheduler(scheduler_schedule, m_core.scheduler_stack);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Preemption handling
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void scheduler_preempt_disable(void) {
    m_core.preempt_count++;
}

void scheduler_preempt_enable(void) {
    if (m_core.preempt_count == 1 && m_core.want_reschedule) {
        // the yield will return with preemption enabled
        scheduler_do_call(scheduler_yield_internal);
    } else {
        // enable preemption manually
        --m_core.preempt_count;
        ASSERT(m_core.preempt_count >= 0);
    }
}

bool scheduler_is_preempt_disabled(void) {
    return m_core.preempt_count != 0;
}
