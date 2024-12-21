#pragma once

#include <arch/idt.h>
#include <mem/memory.h>

#include "lib/defs.h"

#include <stdatomic.h>
#include <lib/list.h>
#include <sync/parking_lot.h>
#include <sync/spinlock.h>

#include "eevdf.h"
#include "runnable.h"

typedef void (*thread_entry_t)(void *arg);

typedef struct thread {
    // The thread name, not null terminated
    char name[256];

    // the runnable of this thread, to queue on the scheduler
    runnable_t runnable;

    // either a freelist link or the scheduler link
    list_t link;

    // The actual stack of the thread
    void* stack_start;
    void* stack_end;

    // the entry point to actually run
    void* arg;
    thread_entry_t entry;

    // The node for the scheduler
    eevdf_node_t scheduler_node;

    //
    // Parking lot context
    //

    // The key that this thread is sleeping on. This may change if the thread
    // is requeued to a different key
    _Atomic(size_t) park_key;

    // The next thread in the parked queue
    struct thread* park_next_in_queue;

    // Token passed to this thread when it is unparked
    size_t unpark_token;

    // Token set by the thread when it is parked
    size_t park_token;

    // Is this thread parked with timeout
    bool parked_with_timeout;

    // parking lot has seen this thread and initialized itself accordingly
    bool parking_lot_seen;
} __attribute__((aligned(4096))) thread_t;

STATIC_ASSERT(sizeof(thread_t) <= SIZE_8MB);

/**
 * The hard-threads are allocated from this place
 */
#define THREADS ((thread_t*)THREADS_ADDR)

/**
* Create a new thread, you need to schedule it yourself
*/
thread_t* thread_create(thread_entry_t callback, void *arg, const char* name_fmt, ...);

/**
 * Free the given thread, returning it to the freelist
 */
void thread_free(thread_t* thread);

/**
* Exit from the thread right now
*/
void thread_exit();
