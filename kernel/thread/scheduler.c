#include "scheduler.h"

#include <arch/intrin.h>
#include <arch/smp.h>
#include <mem/alloc.h>
#include <mem/stack.h>
#include <time/tsc.h>

#include "pcpu.h"
#include "thread.h"

typedef struct scheduler_cpu_context {
    // the scheduler context
    runnable_t scheduler;

    thread_t* current_thread;

    // when set to true preemption should not switch the context
    // but should set the want preemption flag instead
    bool disable_preemption;

    // we got an preemption request while preempt count was 0
    // next time we enable preemption make sure to preempt
    bool want_preemption;
} scheduler_cpu_context_t;

/**
 * The current cpu's context
 */
static CPU_LOCAL scheduler_cpu_context_t m_scheduler_context = {};

/**
 * The scheduler contexts of all cpus
 */
static scheduler_cpu_context_t** m_all_schedulers = NULL;

err_t scheduler_init(void) {
    err_t err = NO_ERROR;

    m_all_schedulers = mem_alloc(g_cpu_count * sizeof(scheduler_cpu_context_t*));
    CHECK_ERROR(m_all_schedulers != NULL, ERROR_OUT_OF_MEMORY);

cleanup:
    return err;
}

err_t scheduler_init_per_core(void) {
    err_t err = NO_ERROR;

    // save the pointer of the current process
    m_all_schedulers[get_cpu_id()] = &m_scheduler_context;

    void* stack = small_stack_alloc();
    CHECK_ERROR(stack != NULL, ERROR_OUT_OF_MEMORY);
    runnable_set_rsp(&m_scheduler_context.scheduler, stack);

cleanup:
    return err;
}

thread_t* scheduler_get_current_thread(void) {
    return m_scheduler_context.current_thread;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Scheduler invocation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef void (*scheduler_func_t)(void);

/**
 * Performs a scheduler call, must be done with preemption disabled
 */
static void scheduler_do_call(scheduler_func_t callback) {
    runnable_set_rip(&m_scheduler_context.scheduler, callback);
    runnable_switch(&m_scheduler_context.current_thread->runnable, &m_scheduler_context.scheduler);
}

/**
 * Perform a scheduler call, this will disable preemption between
 * the calls to make sure we won't get any weird double switch
 */
static void scheduler_call(scheduler_func_t callback) {
    m_scheduler_context.disable_preemption = true;
    scheduler_do_call(callback);
    m_scheduler_context.disable_preemption = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual scheduler
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Lock to protect the scheduler queue
 */
static spinlock_t m_scheduler_lock = INIT_SPINLOCK();

/**
 * Scheduler queue
 */
static list_t m_scheduler_queue = LIST_INIT(&m_scheduler_queue);

void scheduler_wakeup_thread(thread_t* thread) {
    spinlock_lock(&m_scheduler_lock);
    list_add_tail(&m_scheduler_queue, &thread->link);
    spinlock_unlock(&m_scheduler_lock);
}

static void scheduler_schedule(void) {
    // pop a thread from the queue, if null then go and do the same
    spinlock_lock(&m_scheduler_lock);
    list_entry_t* thread_link = list_pop(&m_scheduler_queue);
    spinlock_unlock(&m_scheduler_lock);
    if (thread_link == NULL) {
        // if not running anything, then go to idle
        if (m_scheduler_context.current_thread == NULL) {
            // TODO: something
            asm("cli");
            asm("hlt");
        }

        // otherwise we can return right now
        return;
    }

    // get the actual thread
    thread_t* thread = containerof(thread_link, thread_t, link);

    // save the current thread, if any
    thread_t* current = m_scheduler_context.current_thread;
    if (current != NULL) {
        spinlock_lock(&m_scheduler_lock);
        list_add_tail(&m_scheduler_queue, &current->link);
        spinlock_unlock(&m_scheduler_lock);
    }
    // TODO: save fpu context

    // set the current as this one
    m_scheduler_context.current_thread = thread;

    // resume the thread now
    // TODO: restore fpu context
    runnable_resume(&thread->runnable);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Scheduler API
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void scheduler_yield(void) {
    scheduler_call(scheduler_schedule);
}

void scheduler_start_per_core(void) {
    // enable interrupts at this point
    asm("sti");

    // set the scheduler_schedule as target
    runnable_set_rip(&m_scheduler_context.scheduler, scheduler_schedule);

    // use the jump since we don't have a valid thread right now
    runnable_resume(&m_scheduler_context.scheduler);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Preemption handling
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void scheduler_disable_preemption(void) {
    m_scheduler_context.disable_preemption = true;
}

void scheduler_enable_preemption(void) {
    // if we got a preemption request we need to call the scheduler, we already are
    // without preemption so no need to use the normal scheduler_call
    if (m_scheduler_context.want_preemption) {
        scheduler_do_call(scheduler_schedule);
    }

    // and now disable preemption now that we return
    m_scheduler_context.disable_preemption = false;
}
