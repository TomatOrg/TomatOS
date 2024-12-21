#pragma once

#include <stdnoreturn.h>

typedef struct runnable {
    // The rsp of the task, as part of context switch
    // we just switch to this stack and ret, any context
    // must be saved to the stack of it
    void* rsp;
} runnable_t;

/**
 * Initialize a runnable
 */
void runnable_set_rsp(runnable_t* to, void* rsp);

/**
 * Set the instruction pointer the runnable will continue at
 */
void runnable_set_rip(runnable_t* to, void* rip);

/**
 * Switch from one runnable to another, the from
 * will be signaled as ready while the to is assumed
 * to already be ready
 */
void runnable_switch(runnable_t* from, runnable_t* to);

/**
 * Jump into a runnable, this will completely ignore whatever
 * is currently running
 */
noreturn void runnable_resume(runnable_t* to);
