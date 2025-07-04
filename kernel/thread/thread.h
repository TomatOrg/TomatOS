#pragma once

#include <arch/intr.h>
#include <mem/memory.h>

#include "lib/defs.h"

#include <stdatomic.h>
#include <lib/list.h>
#include <sync/spinlock.h>
#include <stdnoreturn.h>

typedef void (*thread_entry_t)(void *arg);

typedef enum thread_status {
    /**
     * The thread was just allocated and has
     * not yet been initialized
     */
    THREAD_STATUS_IDLE,

    /**
     * The thread is queued in the scheduler, and
     * is not currently running code
     */
    THREAD_STATUS_RUNNABLE,

    /**
     * The thread is currently running, it is not
     * queued in the scheduler
     */
    THREAD_STATUS_RUNNING,

    /**
     * The thread is blocked, it is not in a run queue
     * and it is not running code.
     */
    THREAD_STATUS_WAITING,

    /**
     * The thread is dead, it may be on the freelist at this point
     */
    THREAD_STATUS_DEAD,
} thread_status_t;

/**
 * The saved state of the thread as it
 * switches to the scheduler
 */
typedef struct thread_frame {
    // we are going to save all the registers just for easier
    // debugging, even tho we could just save only the
    // callee-saved registers
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;

    // this is to ensure a nice stack unwind
    uint64_t rbp;
    uint64_t rip;
} thread_frame_t;

typedef struct thread {
    // The thread name, not null terminated
    char name[256];

    // the status of the current thread
    _Atomic(thread_status_t) status;

    // either a freelist link or the scheduler link
    list_t link;

    // The actual stack of the thread
    void* stack_start;
    void* stack_end;

    // The node for the scheduler
    list_entry_t scheduler_node;

    // The CPU state of the thread
    thread_frame_t* cpu_state;

    // The extended state of the thread, must be aligned
    // for XSAVE to work
    __attribute__((aligned(64)))
    uint8_t extended_state[];
} __attribute__((aligned(4096))) thread_t;

STATIC_ASSERT(sizeof(thread_t) <= SIZE_8MB);

/**
 * The hard-threads are allocated from this place
 */
#define THREADS ((thread_t*)THREADS_ADDR)

/**
 * Calculate the ID of the thread
 */
static inline size_t get_thread_id(thread_t* thread) { return thread - THREADS; }

/**
 * Switch the thread status, ensuring we first arrive at the status
 * we want before we try to do anything
 *
 * @param old_value     [IN] The old value we want to see before we continue
 * @param new_value     [IN] The new value we want to have
 */
void thread_switch_status(thread_t* thread, thread_status_t old_value, thread_status_t new_value);

/**
* Create a new thread, you need to schedule it yourself
*/
thread_t* thread_create(thread_entry_t callback, void *arg, const char* name_fmt, ...);

/**
 * Resume a thread, destroying the
 * current context that we have
 */
noreturn void thread_resume(thread_t* thread);

/**
 * Save the extended state of the thread
 */
void thread_save_extended_state(thread_t* thread);

/**
 * Free the given thread, returning it to the freelist
 */
void thread_free(thread_t* thread);

/**
* Exit from the thread right now
*/
void thread_exit();
