#pragma once
#include <lib/except.h>

#include "thread.h"

/**
 * Initialize the core of the scheduler
 */
err_t scheduler_init(void);

/**
 * Initialize the scheduler on each core, must be called
 * before we start the scheduler
 */
err_t scheduler_init_per_core(void);

/**
 * Start the scheduler on the current core
 */
void scheduler_start_per_core(void);

//----------------------------------------------------------------------------------------------------------------------
// Primitives over any thread
//----------------------------------------------------------------------------------------------------------------------

/**
 * Start a new thread
 */
void scheduler_start_thread(thread_t* thread);

/**
 * Wakeup a thread and let it run
 */
void scheduler_wakeup_thread(thread_t* thread);

//----------------------------------------------------------------------------------------------------------------------
// Primitives on the current thread
//----------------------------------------------------------------------------------------------------------------------

/**
 * Get the currently running thread
 */
thread_t* scheduler_get_current_thread(void);

/**
 * Yield to the next task right now 
 */
void scheduler_yield(void);

/**
 * Park the current thread
 */
void scheduler_park(void);

/**
 * Park the current thread
 */
void scheduler_exit(void);

/**
 * Scheduler preemption handling
 */
void scheduler_preempt(void);

//----------------------------------------------------------------------------------------------------------------------
// Preemption handling
//----------------------------------------------------------------------------------------------------------------------

/**
 * Disable preemption
 */
void scheduler_preempt_disable(void);

/**
 * Enable preemption after disabling it
 */
void scheduler_preempt_enable(void);
