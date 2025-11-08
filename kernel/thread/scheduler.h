#pragma once
#include <lib/except.h>

#include "thread.h"

/**
 * Initialize the core of the scheduler
 */
err_t init_scheduler(void);

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
 * A callback to run after we set the thread to sleep properly
 * but before anyone can actually wake it up
 */
typedef bool (*scheduler_park_callback_t)(void* arg);

/**
 * Park the current thread
 */
void scheduler_park(scheduler_park_callback_t callback, void* arg);

/**
 * Park the current thread
 */
void scheduler_exit(void);

/**
 * Request a reschedule, if the preempt count is positive this will be delayed until the preempt
 * count reaches zero
 */
void scheduler_reschedule(void);

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

/**
 * Is preemption currently disabled
 */
bool scheduler_is_preempt_disabled(void);
