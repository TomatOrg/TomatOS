#include "irq.h"
#include "dotnet/gc/heap.h"
#include "thread/scheduler.h"

typedef struct irq_instance {
    // The IRQ ops to mask/unmask the irq, NULL if
    // the entry is not allocated
    irq_ops_t* ops;

    // context for it
    void* ctx;

    // The thread waiting on this irq, NULL if non
    thread_t* waiting_thread;
} irq_instance_t;

// TODO: per-cpu irq

// The irq entries
static irq_instance_t m_irqs[IRQ_ALLOC_END - IRQ_ALLOC_BASE] = {};

// Sync the subsystem
static spinlock_t m_irq_spinlock = INIT_SPINLOCK();

err_t alloc_irq(int count, irq_ops_t* ops, void* ctx, uint8_t* vector) {
    err_t err = NO_ERROR;

    spinlock_lock(&m_irq_spinlock);

    int found = 0;
    int entry = 0;
    for (entry = 0; entry < ARRAY_LEN(m_irqs); entry++) {
        while (found != count) {
            // check if entry is allocated, if it
            // is, ignore it
            if (m_irqs[entry].ops != NULL) {
                found = 0;
                break;
            }

            // it is fine, increase by one
            found++;
        }
    }

    // make sure we actually found an entry
    CHECK(found == count);

    // go backwards and set all the entries
    while (count--) {
        m_irqs[entry--].ops = ops;
    }

    // set the base
    *vector = entry;

cleanup:
    spinlock_unlock(&m_irq_spinlock);

    return err;
}

void irq_wait(uint8_t handler) {
    ASSERT(handler < ARRAY_LEN(m_irqs));
    ASSERT(m_irqs[handler].ops != NULL);

    // set the waiting thread to us
    irq_instance_t* instance = &m_irqs[handler];
    instance->waiting_thread = get_current_thread();

    // park the current thread, will wake it up laterinstnace.
    scheduler_park(instance->ops->unmask, instance->ctx);
}

void irq_dispatch(interrupt_context_t* ctx) {
    int handler = ctx->int_num - IRQ_ALLOC_BASE;

    irq_instance_t* instance = &m_irqs[handler];
    if (instance->ops == NULL) {
        WARN("irq: got IRQ #%d which has no handler, badly configured device?", ctx->int_num);
        return;
    }

    if (instance->waiting_thread == NULL) {
        WARN("irq: got IRQ #%d while no thread is waiting, invalid mask function?", ctx->int_num);
    }

    // ready the thread
    scheduler_schedule_thread(ctx, instance->waiting_thread);

    // set the instance to NULL
    instance->waiting_thread = NULL;

    // mask the irq
    instance->ops->mask(instance->ctx);
}