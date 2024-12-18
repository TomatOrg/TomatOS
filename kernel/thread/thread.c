#include "thread.h"

#include <arch/gdt.h>
#include <lib/list.h>
#include <lib/string.h>
#include <sync/spinlock.h>

#include "scheduler.h"

/**
 * The amount of allocated threads we have
 */
static size_t m_thread_top = 0;

/**
 * Freelist of threads structs not in use
 */
static list_t m_thread_freelist = LIST_INIT(&m_thread_freelist);

/**
 * Protect the thread list and the thread top variables
 */
static spinlock_t m_thread_freelist_lock = INIT_SPINLOCK();

static thread_t* thread_alloc() {
    thread_t* thread = NULL;

    spinlock_lock(&m_thread_freelist_lock);

    // try to get an already free thread
    list_entry_t* entry = list_pop(&m_thread_freelist);
    if (entry != NULL) {
        thread = containerof(entry, thread_t, link);
        memset(thread, 0, sizeof(*thread));
    }

    // increase the thread pool
    if (thread == NULL && m_thread_top < UINT16_MAX) {
        thread = &THREADS[m_thread_top];
        m_thread_top++;

        // initialize anything that it needs, we multiply after the ++ because we want to get
        // the top of the stack, not the bottom of it
        thread->stack_start = (void*)((STACKS_ADDR));
        thread->stack_end = (void*)((STACKS_ADDR + SIZE_8MB * m_thread_top));
    }

    spinlock_unlock(&m_thread_freelist_lock);

    return thread;
}

static void thread_entry() {
    thread_t* thread = scheduler_get_current_thread();
    thread->entry(thread->arg);
    thread_exit();
}

thread_t* thread_create(thread_entry_t callback, void* arg) {
    thread_t* thread = thread_alloc();
    if (thread == NULL) {
        return NULL;
    }

    // initialize the callback, this will be used by the thread_entry to
    // call the real entry point
    thread->entry = callback;
    thread->arg = arg;

    // set the thread entry as the first function to run
    // it will call the callback properly with its argument
    runnable_set_rsp(&thread->runnable, thread->stack_end);
    runnable_set_rip(&thread->runnable, thread_entry);

    // TODO: fpu context

    return thread;
}

void thread_free(thread_t* thread) {
    memset(thread, 0, sizeof(*thread));

    spinlock_lock(&m_thread_freelist_lock);
    list_add(&m_thread_freelist, &thread->link);
    spinlock_unlock(&m_thread_freelist_lock);
}

void thread_exit() {
    // TODO: something in here
}
