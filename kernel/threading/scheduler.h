#pragma once

#include "thread.h"

#include <arch/idt.h>

#include <stdbool.h>

/**
 * Helper method, check if the thread should spin in the given
 * iteration in a row. Used by the mutex
 *
 * @param i     [IN] Iteration
 */
bool scheduler_can_spin(int i);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get the current running thread
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Put a thread into a ready state
 *
 * @param thread    [IN]
 */
void scheduler_ready_thread(thread_t* thread);

typedef struct suspend_state {
    thread_t* thread;
    bool stopped;
    bool dead;
} suspend_state_t;

/**
 *
 *
 * @param thread
 */
suspend_state_t scheduler_suspend_thread(thread_t* thread);

/**
 *
 * @param thread
 */
void scheduler_resume_thread(suspend_state_t status);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks from interrupts to the scheduler
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void scheduler_on_schedule(interrupt_context_t* ctx);

void scheduler_on_yield(interrupt_context_t* ctx);

void scheduler_on_park(interrupt_context_t* ctx);

void scheduler_on_drop(interrupt_context_t* ctx);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Call the scheduler to do stuff
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Request the scheduler to schedule instead of the current thread, giving a new time-slice
 * to another thread, putting us into the global run-queue
 */
void scheduler_schedule();

/**
 * Request the scheduler to yield from our thread, passing our time-slice to the caller,
 * putting us at the CPU's local run-queue
 */
void scheduler_yield();

/**
 * Park the current thread, putting us into sleep and not putting us to the run-queue
 */
void scheduler_park();

/**
 * Drop the current thread and schedule a new one instead
 */
void scheduler_drop_current();

/**
 * Startup the scheduler
 */
void scheduler_startup();

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get the current running thread
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Get the currently running thread on the current CPU
 */
thread_t* get_current_thread();
