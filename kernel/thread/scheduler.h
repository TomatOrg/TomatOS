#pragma once

#include "thread.h"
#include "lib/except.h"

err_t scheduler_init();

/**
 * Initialize the scheduler
 */
void scheduler_init_per_core();

/**
 * Called to startup the scheduler per core
 */
void scheduler_start_per_core();

/**
 * Wakeup the given thread for scheduling
 */
void scheduler_wakeup_thread(thread_t* thread);

/**
 * Yield from the current thread
 */
void scheduler_yield();

/**
 * Handle a scheduler interrupt
 */
void scheduler_interrupt(interrupt_context_t* ctx);

/**
 * Gets the currently running thread
 */
thread_t* get_current_thread();
