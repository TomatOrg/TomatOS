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
    int entry_base = 0;
    for (int entry = 0; entry < ARRAY_LEN(m_irqs) + 1; entry++) {
        if (m_irqs[entry].ops != NULL) {
            // bad entry, we will try the next one
            entry_base = entry + 1;
            continue;
        }

        // this entry is good
        found++;

        // we found enough entries
        if (found == count) {
            break;
        }
    }

    // make sure we actually found an entry
    CHECK(found == count);

    // set all the entries to this
    for (int i = 0; i < count; i++) {
        m_irqs[entry_base + i].ops = ops;
        m_irqs[entry_base + i].ctx = ctx;
    }

    // return as a vector
    *vector = entry_base + IRQ_ALLOC_BASE;

cleanup:
    spinlock_unlock(&m_irq_spinlock);

    return err;
}

void irq_wait(uint8_t handler) {
    // turn the vector to an index
    ASSERT(IRQ_ALLOC_BASE <= handler && handler <= IRQ_ALLOC_END);
    handler -= IRQ_ALLOC_BASE;

    // check the index range just in case
    ASSERT(handler < ARRAY_LEN(m_irqs));
    ASSERT(m_irqs[handler].ops != NULL);

    // set the waiting thread to us
    irq_instance_t* instance = &m_irqs[handler];
    instance->waiting_thread = get_current_thread();

    // park the current thread, will wake it up laterinstnace.
    scheduler_park(instance->ops->unmask, instance->ctx);
}

void irq_dispatch(interrupt_context_t* ctx) {
    // just in case
    ASSERT(IRQ_ALLOC_BASE <= ctx->int_num && ctx->int_num <= IRQ_ALLOC_END);
    int handler = ctx->int_num - IRQ_ALLOC_BASE;

    irq_instance_t* instance = &m_irqs[handler];
    if (instance->ops == NULL) {
        WARN("irq: got IRQ #%d which has no handler, badly configured device?", ctx->int_num);
        return;
    }

    if (instance->waiting_thread == NULL) {
        WARN("irq: got IRQ #%d while no thread is waiting, invalid mask function?", ctx->int_num);
    }

    // schedule the thread right now
    scheduler_schedule_thread(ctx, instance->waiting_thread);

    // no one is waiting on this anymore
    instance->waiting_thread = NULL;

    // mask the irq
    instance->ops->mask(instance->ctx);
}
