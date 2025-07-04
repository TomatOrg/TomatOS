#include "thread.h"

#include <arch/gdt.h>
#include <arch/intrin.h>
#include <lib/list.h>
#include <lib/printf.h>
#include <lib/string.h>
#include <sync/spinlock.h>

#include "scheduler.h"
#include "time/tsc.h"

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
static spinlock_t m_thread_freelist_lock = SPINLOCK_INIT;

static thread_t* thread_alloc() {
    thread_t* thread = NULL;

    spinlock_acquire(&m_thread_freelist_lock);

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

        // switch to a dead state, just so we can wake it up properly
        thread_switch_status(thread, THREAD_STATUS_IDLE, THREAD_STATUS_DEAD);
    }

    spinlock_release(&m_thread_freelist_lock);

    return thread;
}

void thread_switch_status(thread_t* thread, thread_status_t old_value, thread_status_t new_value) {
    thread_status_t current = old_value;
    for (int i = 0; !atomic_compare_exchange_strong(&thread->status, &current, new_value); current = old_value, i++) {
        ASSERT(!(old_value == THREAD_STATUS_WAITING && current == THREAD_STATUS_RUNNABLE), "waiting for WAITING but is RUNNABLE");

        // TODO: the go code this is inspired by has some yield mechanism, can we use it? do we want to?
    }
}

__attribute__((force_align_arg_pointer))
static void thread_entry() {
    // and now run the
    thread_t* thread = scheduler_get_current_thread();
    thread->entry(thread->arg);
    thread_exit();
}

thread_t* thread_create(thread_entry_t callback, void* arg, const char* name_fmt, ...) {
    thread_t* thread = thread_alloc();
    if (thread == NULL) {
        return NULL;
    }
    ASSERT(thread->status == THREAD_STATUS_DEAD);

    // set the name
    va_list va;
    va_start(va, name_fmt);
    ksnprintf(thread->name, sizeof(thread->name) - 1, name_fmt, va);
    va_end(va);

    // initialize the callback, this will be used by the thread_entry to
    // call the real entry point
    thread->entry = callback;
    thread->arg = arg;

    // set the thread entry as the first function to run
    // it will call the callback properly with its argument
    thread->cpu_state = thread->stack_end - sizeof(*thread->cpu_state);
    thread->cpu_state->rip = (uintptr_t)thread_entry;

    // setup the extended state
    xsave_legacy_region_t* extended_state = (xsave_legacy_region_t*)thread->extended_state;
    extended_state->mxscr = 0x00001f80;

    // we are going to start it in a parked state, and the caller needs
    // to actually queue it
    thread_switch_status(thread, THREAD_STATUS_DEAD, THREAD_STATUS_WAITING);

    return thread;
}

/**
 * Finalizes the switch to the thread, including
 * actually jumping to it
 */
noreturn void thread_resume_finish(thread_frame_t* frame);

void thread_resume(thread_t* thread) {
    // Restore the extended state
    // TODO: support for xrstors when available
    __builtin_ia32_xrstor64(thread->extended_state, ~0ull);

    // and now we can jump to the thread
    thread_resume_finish(thread->cpu_state);
}

void thread_save_extended_state(thread_t* thread) {
    // Save the extended state
    // TODO: support for using xsaves which has both init and modified and compact
    //       optimizations, we won't support xsavec since it does not have the modified
    //       optimization
    __builtin_ia32_xsaveopt64(thread->extended_state, ~0ull);
}

void thread_free(thread_t* thread) {
    memset(thread, 0, sizeof(*thread));

    spinlock_acquire(&m_thread_freelist_lock);
    list_add(&m_thread_freelist, &thread->link);
    spinlock_release(&m_thread_freelist_lock);
}

void thread_exit() {
    scheduler_exit();
}
