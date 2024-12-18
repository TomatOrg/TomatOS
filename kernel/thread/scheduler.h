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

/**
 * Get the currently running thread
 */
thread_t* scheduler_get_current_thread(void);

/**
 * Yield to the next task right now 
 */
void scheduler_yield(void);

/**
 * Disable preemption
 */
void scheduler_disable_preemption(void);

/**
 * Enable preemption after disabling it
 */
void scheduler_enable_preemption(void);
