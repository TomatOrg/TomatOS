#pragma once

#include "sync/spinlock.h"
#include "arch/idt.h"

typedef enum thread_status {
    /**
     * Means this thread was just allocated and has not
     * yet been initialized
     */
    THREAD_STATUS_IDLE,

    /**
     * Means this thread is on a run queue. It is
     * not currently executing user code.
     */
    THREAD_STATUS_RUNNABLE,

    /**
     * Means this thread may execute user code.
     */
    THREAD_STATUS_RUNNING,

    /**
     * Means this thread is blocked in the runtime.
     * It is not executing user code. It is not on a run queue,
     * but should be recorded somewhere so it can be scheduled
     * when necessary.
     */
    THREAD_STATUS_WAITING,

    /**
     * Means the thread stopped itself for a suspend
     * preemption. IT is like THREAD_STATUS_WAITING, but
     * nothing is yet responsible for readying it. some
     * suspend must CAS the status to THREAD_STATUS_WAITING
     * to take responsibility for readying this thread
     */
    THREAD_STATUS_PREEMPTED,

    /**
     * Means this thread is currently unused. It may be
     * just exited, on a free list, or just being initialized.
     * It is not executing user code.
     */
    THREAD_STATUS_DEAD,

    /**
     * Indicates someone wants to suspend this thread (probably the
     * garbage collector).
     */
    THREAD_SUSPEND = 0x1000,
} thread_status_t;

typedef struct thread {
    // the thread name
    char name[64];

    //
    // The thread context
    //

    // gprs
    interrupt_context_t ctx;

    // thread control block
    uintptr_t tcb;

    //
    // scheduling related
    //

    // preemption signal
    bool preempt;

    // transition to THREAD_STATUS_PREEMPTED on preemption, otherwise just deschedule
    bool preempt_stop;

    // The current status of the thread
    thread_status_t status;

    // Link for the scheduler
    struct thread* sched_link;

    // a spinlock we want to unlock once we start waiting
    spinlock_t* wait_lock;
} thread_t;

typedef struct waiting_thread {
    thread_t* thread;

    // only used in the cache
    struct waiting_thread* next;

    uint32_t ticket;

    struct waiting_thread* wait_link;
    struct waiting_thread* wait_tail;
} waiting_thread_t;

/**
 * For thread-locals
 */
#define THREAD_LOCAL _Thread_local

/**
 * Acquire a new waiting thread descriptor
 */
waiting_thread_t* acquire_waiting_thread();

/**
 * Release a waiting thread descriptor
 *
 * @param wt    [IN] The WT descriptor
 */
void release_waiting_thread(waiting_thread_t* wt);

typedef void(*thread_entry_t)(void* ctx);

thread_t* create_thread(thread_entry_t entry, void* ctx, const char* fmt, ...);

/**
 * Called upon a thread exit
 *
 * @param thread    [IN] The thread that exited
 */
void thread_exit(thread_t* thread);

/**
 * Get the status of a thread atomically
 *
 * @param thread    [IN] The target thread
 */
thread_status_t get_thread_status(thread_t* thread);

/**
 * Compare and swap the thread state atomically
 *
 * @remark
 * This will suspend until the thread status is equals to old and only then try to
 * set it to new, if that fails it will continue to try until it has a success.
 *
 * @param thread    [IN] The target thread
 * @param old       [IN] The old status
 * @param new       [IN] The new status
 */
void cas_thread_state(thread_t* thread, thread_status_t old, thread_status_t new);
