#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>

typedef void (*timer_func_t)(void* arg, uintptr_t now);

typedef enum timer_status {
    TIMER_NO_STATUS,

    // Waiting for timer to fire.
    // The timer is in some CPU's heap.
    TIMER_WAITING,

    // Running the timer function.
    // A timer will only have this status briefly.
    TIMER_RUNNING,

    // The timer is deleted and should be removed.
    // It should not be run, but it is still in some CPU's heap.
    TIMER_DELETED,

    // The timer is being removed.
    // The timer will only have this status briefly.
    TIMER_REMOVING,

    // The timer has been stopped.
    // It is not in any CPU's heap.
    TIMER_REMOVED,

    // The timer is being modified.
    // The timer will only have this status briefly.
    TIMER_MODIFYING,

    // The timer has been modified to an earlier time.
    // The new when value is in the nextwhen field.
    // The timer is in some P's heap, possibly in the wrong place.
    TIMER_MODIFIED_EARLIER,

    // The timer has been modified to the same or a later time.
    // The new when value is in the nextwhen field.
    // The timer is in some P's heap, possibly in the wrong place.
    TIMER_MODIFIED_LATER,

    // The timer has been modified and is being moved.
    // The timer will only have this status briefly.
    TIMER_MOVING,
} timer_status_t;

typedef struct timer {
    // If this timer is on a heap, which CPU's heap it is on.
    int cpu;

 	// Timer wakes up at when, and then at when+period, ... (period > 0 only)
	// each time calling func(arg, now) in the timer thread, so func must be
	// a well-behaved function and not block. Values are in microsecond
	//
	// when must be positive on an active timer.
    int64_t when;
    int64_t period;
    timer_func_t func;
    void* arg;
    uintptr_t seq;

    // When to set the when field to in timerModifiedXX status.
    int64_t nextwhen;

    // The status field holds one of the values below
    _Atomic(timer_status_t) status;

    // how many references we have for this
    atomic_size_t ref_count;
} timer_t;

/**
 * Create a new timer
 *
 * @remark
 * After you create it you should set the following fields:
 *  - when: when the timer should stop
 *  - func+arg: for the callback
 *  - period (optionally): if you want this timer to happen more than once
 */
timer_t* create_timer();

void timer_start(timer_t* timer);

bool timer_stop(timer_t* timer);

/**
 * Resets the time when a timer should fire.
 *
 * If used for an inactive timer, the timer will become active.
 *
 * This should be called instead of add_timer if the timer value has been,
 * or may have been, used previously.
 *
 * @return Was the timer modified before it was run.
 */
bool timer_reset(timer_t* timer, int64_t when);

/**
 *
 *
 * @return Was the timer modified before it was run.
 */
bool timer_modify(timer_t* timer, int64_t when, int64_t period, timer_func_t func, void* arg, uintptr_t seq);

/**
 * Increments the reference count of the timer
 */
timer_t* put_timer(timer_t* timer);

/**
 * Decrements the reference count, will be deleted when reference count reaches 0
 */
void release_timer(timer_t* timer);

#define SAFE_RELEASE_TIMER(timer) \
    do { \
        if (timer != NULL) { \
            release_timer(timer); \
            timer = NULL; \
        } \
    } while (0)

void check_timers(int cpu, int64_t* now, int64_t* poll_until, bool* ran);

/**
 * Checks if the given CPU has any timers
 */
bool cpu_has_timers(int cpu);

/**
 * Set that the cpu has timers
 */
void set_has_timers(int cpu);

/**
 * Update the timer mask of the local cpu
 */
void update_cpu_timers_mask();

/**
 * Looks at a CPU's timers and returns the time when
 * we should wake up the poller. It returns 0 if there are no timers.
 */
int64_t nobarrier_wake_time(int cpu);
