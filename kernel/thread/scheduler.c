#include "scheduler.h"

#include <arch/intrin.h>
#include <arch/smp.h>
#include <mem/alloc.h>
#include <sync/spinlock.h>
#include <time/timer.h>

#include "pcpu.h"

typedef struct scheduler_cpu_context {
    // lock on the run queue
    spinlock_t run_queue_lock;

    // the current state of the scheduler
    volatile bool wakeup_trigger;

    // the run-queue of threads
    list_t run_queue;
} scheduler_cpu_context_t;

/**
 * The list of cpu contexts
 */
static scheduler_cpu_context_t* m_scheduler_contexts;

/**
 * The current cpu context
 */
static CPU_LOCAL scheduler_cpu_context_t m_current_context;

/**
 * The currently running thread
 */
static CPU_LOCAL thread_t* m_current_thread = NULL;

err_t scheduler_init() {
    err_t err = NO_ERROR;

    m_scheduler_contexts = mem_alloc(sizeof(scheduler_cpu_context_t) * g_cpu_count);
    CHECK(m_scheduler_contexts != NULL);

    // TODO: test for HFI/Thread Director support

cleanup:
    return err;
}

void scheduler_init_per_core() {
    // set the context of the current cpu
    m_current_context = m_scheduler_contexts[get_cpu_id()];
    list_init(&m_current_context.run_queue);
}

void scheduler_start_per_core() {
    // set the deadline to right now so it will trigger right away
    scheduler_yield();

    // and now enable interrupts and start
    asm("sti");
    asm("hlt");
}

void scheduler_wakeup_thread(thread_t* thread) {
    thread->status = THREAD_STATUS_READY;
    list_add_tail(&m_current_context.run_queue, &thread->link);
}

void scheduler_yield() {
    // just trigger the scheduler interrupt and let it run
    timer_set_deadline(1);
}

thread_t* get_current_thread() {
    return m_current_thread;
}

void scheduler_interrupt(interrupt_context_t* ctx) {
    // save the thread that was running
    if (m_current_thread != NULL) {
        if (m_current_thread->status == THREAD_STATUS_RUNNING) {
            // thread was running, return to queue
            m_current_thread->cpu_context = *ctx;
            m_current_thread->status = THREAD_STATUS_READY;

            // add the thread to the end of the run queue
            spinlock_lock(&m_current_context.run_queue_lock);
            list_add_tail(&m_current_context.run_queue, &m_current_thread->link);
            spinlock_unlock(&m_current_context.run_queue_lock);

        } else if (m_current_thread->status == THREAD_STATUS_DEAD) {
            // the thread is dead, just free it
            thread_free(m_current_thread);
        }

        m_current_thread = NULL;
    }

    for (;;) {
        // take a thread from the run-queue
        spinlock_lock(&m_current_context.run_queue_lock);
        list_entry_t* entry = list_pop(&m_current_context.run_queue);
        spinlock_unlock(&m_current_context.run_queue_lock);

        // if we got something run it
        if (entry != NULL) {
            thread_t* thread = containerof(entry, thread_t, link);
            *ctx = thread->cpu_context;
            thread->status = THREAD_STATUS_RUNNING;
            m_current_thread = thread;
            return;
        }

        // first reset the trigger for waking up
        m_current_context.wakeup_trigger = false;

        // now setup the monitor, we are going to check that we have the
        // value that we expect, in case between now and the monitor something
        // has changed, and as the intel manual says we are going to check
        // again and then actually mwait
        // TODO: use umwait if supported so we can wakeup on a timer if any
        uintptr_t eax = (uintptr_t)&m_current_context.wakeup_trigger;
        uintptr_t ecx = BIT1;
        uintptr_t edx = 0;
        if (!m_current_context.wakeup_trigger) {
            __monitor(eax, ecx, edx);
            if (!m_current_context.wakeup_trigger) {
                __mwait(eax, ecx);
            }
        }
    }
}
