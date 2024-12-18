#pragma once

#include <arch/idt.h>
#include <mem/memory.h>

#include "lib/defs.h"

#include <stdatomic.h>
#include <lib/list.h>
#include <sync/spinlock.h>

#include "runnable.h"

typedef void (*thread_entry_t)(void *arg);

typedef enum thread_priority {
    THREAD_PRIORITY_LOWEST = 0,
    THREAD_PRIORITY_BELOW_NORMAL = 1,
    THREAD_PRIORITY_NORMAL = 2,
    THREAD_PRIORITY_ABOVE_NORMAL = 3,
    THREAD_PRIORITY_HIGHEST = 4,

    THREAD_PRIORITY_MAX
} thread_priority_t;

typedef struct thread {
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

    // The thread name, not null terminated
    char name[256];
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
