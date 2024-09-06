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
        thread->stack_top = (void*)((STACKS_ADDR + SIZE_8MB * m_thread_top));
    }

    spinlock_unlock(&m_thread_freelist_lock);

    return thread;
}

thread_t* thread_create(thread_entry_t callback, void* arg) {
    thread_t* thread = thread_alloc();
    if (thread == NULL) {
        return NULL;
    }

    // initialize segments
    thread->cpu_context.cs = GDT_CODE;
    thread->cpu_context.ss = GDT_DATA;

    // set the entry point
    thread->cpu_context.rip = (uintptr_t)callback;
    thread->cpu_context.rdi = (uintptr_t)arg;
    thread->cpu_context.rsp = (uintptr_t)thread->stack_top - 16;

    // set the flags
    thread->cpu_context.rflags.always_one = 1;
    thread->cpu_context.rflags.IF = 1;

    // set the thread exit as the return from the function
    *((void**)(thread->cpu_context.rsp)) = thread_exit;
    *((void**)(thread->cpu_context.rsp + sizeof(uintptr_t))) = 0;

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
    // disable preemption so we can
    thread_t* thread = get_current_thread();
    thread->status = THREAD_STATUS_DEAD;
    scheduler_yield();
    __builtin_trap();
}
