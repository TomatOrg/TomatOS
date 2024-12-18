#include "scheduler.h"

#include <arch/smp.h>
#include <mem/alloc.h>
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

    void* stack = mem_alloc(SIZE_4KB);
    CHECK_ERROR(stack != NULL, ERROR_OUT_OF_MEMORY);
    runnable_set_rsp(&m_scheduler_context.scheduler, stack + SIZE_4KB);

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

static void scheduler_schedule(void) {
    asm("cli");
    asm("hlt");
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
    runnable_jump(&m_scheduler_context.scheduler);
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
