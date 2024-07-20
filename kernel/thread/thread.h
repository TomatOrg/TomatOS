#pragma once

#include <arch/idt.h>
#include <mem/memory.h>

#include "lib/defs.h"

#include <stdatomic.h>
#include <lib/list.h>

typedef enum thread_status {
    /**
     * Thread is sleeping, aka not in run-queue
     */
    THREAD_STATUS_SLEEPING,

    /**
    * The thread is ready to run
    */
    THREAD_STATUS_READY,

    /**
    * The thread is running right now
    */
    THREAD_STATUS_RUNNING,

    /**
    * Thread is dead, free it on next schedule
    */
    THREAD_STATUS_DEAD,
} thread_status_t;

typedef enum thread_priority {
    THREAD_PRIORITY_LOWEST = 0,
    THREAD_PRIORITY_BELOW_NORMAL = 1,
    THREAD_PRIORITY_NORMAL = 2,
    THREAD_PRIORITY_ABOVE_NORMAL = 3,
    THREAD_PRIORITY_HIGHEST = 4,
} thread_priority_t;

typedef struct thread {
    // link in the scheduler or allocation
    list_entry_t link;

    // the status of the current thread
    thread_status_t status;

    //
    // Parking lot context
    //

    // Key that this thread is sleeping on. This may change if the thread is
    // requeued to a different key
    atomic_size_t key;

    // Linked list of parked threads in a bucket
    list_entry_t next_in_queue;

    // Token passed to this thread when it is unparked
    uint64_t unpark_token;

    // Token value set by the thread when it was parked
    uint64_t park_token;

    // Is thie thread parked with a timeout?
    bool parked_with_timeout;

    //
    // Context switching context
    //

    // The bottom of the stack, as allocated by the stack allocator
    void *stack_top;

    // the context of the thread, cpu fpu and what else
    interrupt_context_t cpu_context;
} __attribute__((aligned(4096))) thread_t;

STATIC_ASSERT(sizeof(thread_t) <= SIZE_8MB);

#define THREADS ((thread_t*)THREADS_ADDR)

/**
* Get the id of a thread
*/
static inline uint16_t thread_get_id(const thread_t *thread) {
    return thread - THREADS;
}

typedef void (*thread_entry_t)(void *arg);

/**
* Create a new thread, you need to schedule it yourself
*/
thread_t *thread_create(thread_entry_t callback, void *arg);

/**
 * Free the given thread, returning it to the freelist
 */
void thread_free(thread_t* thread);

/**
* Exit from the thread right now
*/
void thread_exit();
