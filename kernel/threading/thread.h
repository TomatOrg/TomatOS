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
     * Means this thread is currently unused. It may be
     * just exited, on a free list, or just being initialized.
     * It is not executing user code.
     */
    THREAD_STATUS_DEAD,
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
 * Acquire a new waiting thread descriptor
 */
waiting_thread_t* acquire_waiting_thread();

/**
 * Release a waiting thread descriptor
 *
 * @param wt    [IN] The WT descriptor
 */
void release_waiting_thread(waiting_thread_t* wt);

/**
 * Get the status of a thread atomically
 *
 * @param thread    [IN] The target thread
 */
thread_status_t get_thread_status(thread_t* thread);

/**
 * Compare and swap the thread state atomically
 *
 * @param thread    [IN] The target thread
 * @param old       [IN] The old status
 * @param new       [IN] The new status
 */
void cas_thread_state(thread_t* thread, thread_status_t old, thread_status_t new);
