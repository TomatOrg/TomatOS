#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lib/rbtree/rbtree_types.h"

typedef struct timer timer_t;

typedef void (*timer_callback_t)(timer_t* timer);

typedef struct per_core_timers per_core_timers_t;

struct timer {
    // node to the timer tree
    rb_node_t node;

    // the tree this timer is on
    per_core_timers_t* timers;

    // callback to run when finished, returns true if we should reschedule
    timer_callback_t callback;

    // the deadline for when to run
    uint64_t deadline;
};

/**
 * Initialize the timer subsystem
 */
void init_timers(void);

/**
 * Setup a new timer to fire after the timeout
 */
void timer_set(timer_t* timer, timer_callback_t callback, uint64_t tsc_deadline);

/**
 * Cancel a timer to not fire
 *
 * TODO: make it safe to call it from some other core
 */
void timer_cancel(timer_t* timer);

/**
 * Dispatch all the timers that are ready
 */
void timer_dispatch(void);

/**
 * Sleep for the given amount of time
 */
void timer_sleep(uint64_t ms);
