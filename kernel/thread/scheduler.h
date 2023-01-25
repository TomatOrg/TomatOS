#pragma once

#include "thread.h"
#include "cpu_local.h"

#include <util/except.h>
#include <arch/idt.h>

#include <stdbool.h>


// Scheduler-internal state
typedef enum td_states {
    TDS_INACTIVE = 0x0,
    TDS_INHIBITED,
    TDS_CAN_RUN,
    TDS_RUNQ,
    TDS_RUNNING
} td_states_t;

/**
 * Helper method, check if the thread should spin in the given
 * iteration in a row. Used by the mutex
 *
 * @param i     [IN] Iteration
 */
bool scheduler_can_spin(int i);

/**
 * Initialize the scheduler
 */
err_t init_scheduler();

/**
 * Called by each core to init per-core information, not needed on
 * the BSP as it is called by the init_scheduler
 */
err_t init_scheduler_per_core();

/**
 * Wakes up the CPU sleeping in the poller if it isn't going
 * to wake up before the when argument, or it wakes an idle CPU
 * to service timers and the poller isn't one already.
 */
void scheduler_wake_poller(int64_t when);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get the current running thread
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Put a thread into a ready state
 *
 * @param thread    [IN]
 */
void scheduler_ready_thread(thread_t* thread);

/**
 * Schedule a thread right now
 */
void scheduler_schedule_thread(interrupt_context_t* ctx, thread_t* thread);

typedef struct suspend_state {
    thread_t* thread;
    bool stopped;
    bool dead;
} suspend_state_t;

/**
 * suspends the thread at a safe point and returns the
 * state of the suspended thread. The caller gets read access
 * to the thread until it calls resume.
 *
 * @param thread    [IN] The thread to suspend
 */
suspend_state_t scheduler_suspend_thread(thread_t* thread);

/**
 * Resumes a thread that was previously suspended.
 *
 * @param state     [IN] The state of the thread to resume
 */
void scheduler_resume_thread(suspend_state_t status);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Preemption stuff
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Disable preemption, nestable
 */
void scheduler_preempt_disable(void);

/**
 * Enable preemption, nestable
 */
void scheduler_preempt_enable(void);

/**
 * Returns true if preemption is enabled
 */
bool scheduler_is_preemption(void);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks from interrupts to the scheduler
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void scheduler_on_schedule(interrupt_context_t* ctx);

void scheduler_on_park(interrupt_context_t* ctx);

void scheduler_on_drop(interrupt_context_t* ctx);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Call the scheduler to do stuff
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Request the scheduler to yield from our thread, passing our time-slice to the caller,
 * putting us at the CPU's local run-queue
 */
void scheduler_yield();

/**
 * Park the current thread, putting us into sleep and not putting us to the run-queue
 */
void scheduler_park(void(*callback)(void* arg), void* arg);

/**
 * Drop the current thread and schedule a new one instead
 */
void scheduler_drop_current();

/**
 * Startup the scheduler
 */
void scheduler_startup();

/**
 * Fill the current thread with scheduler-specific information 
 */
void sched_new_thread(thread_t* thread);


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get the current running thread
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Get the currently running thread on the current CPU
 */
thread_t* get_current_thread();
