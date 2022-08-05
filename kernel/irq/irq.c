#include "irq.h"
#include "dotnet/gc/heap.h"
#include "thread/scheduler.h"

typedef struct irq_instance {
    // The IRQ ops to mask/unmask the irq, NULL if
    // the entry is not allocated
    irq_ops_t ops;

    // context for it
    void* ctx;

    // The thread waiting on this irq, NULL if none
    thread_t* waiting_thread;

    // flag to see if we're handling it already
    atomic_flag handling;
} irq_instance_t;

// TODO: per-cpu irq

// The irq entries
static irq_instance_t m_irqs[IRQ_ALLOC_END - IRQ_ALLOC_BASE] = { 0 };

// Sync the subsystem
static spinlock_t m_irq_spinlock = INIT_SPINLOCK();

err_t alloc_irq(int count, irq_ops_t ops, void* ctx, uint8_t* vector) {
    err_t err = NO_ERROR;

    spinlock_lock(&m_irq_spinlock);

    int found = 0;
    int entry = 0;
    for (entry = 0; entry < ARRAY_LEN(m_irqs); entry++) {
        bool isempty = m_irqs[entry].ops.mask == NULL && m_irqs[entry].ops.unmask == NULL;
        if (!isempty) found = 0; else found++;
        if (found == count) break;
    }
    // make sure we actually found an entry
    CHECK(found == count);

    entry -= count - 1;
    // go backwards and set all the entries
    for (int i = 0; i < count; i++) {
        m_irqs[entry + i].ops = ops;
        m_irqs[entry + i].ctx = ctx;
    }
    // set the base
    *vector = entry + IRQ_ALLOC_BASE;

cleanup:
    spinlock_unlock(&m_irq_spinlock);
    
    return err;
}
void irq_wait(uint8_t handler) {
    ASSERT(handler >= IRQ_ALLOC_BASE);
    uint8_t idx = handler - IRQ_ALLOC_BASE;
    ASSERT(idx < ARRAY_LEN(m_irqs));
    ASSERT(m_irqs[idx].ops.mask != NULL);
    ASSERT(m_irqs[idx].ops.unmask != NULL);

    // set the waiting thread to us
    irq_instance_t* instance = &m_irqs[idx];
    instance->waiting_thread = get_current_thread();
    atomic_flag_clear(&instance->handling);

    // park the current thread, will wake it up laterinstnace.
    scheduler_park(instance->ops.unmask, instance->ctx);
}

void irq_dispatch(interrupt_context_t* ctx) {
    int handler = ctx->int_num - IRQ_ALLOC_BASE;

    irq_instance_t* instance = &m_irqs[handler];
    
    if (atomic_flag_test_and_set(&instance->handling)) return;

    if (instance->ops.mask == NULL || instance->ops.unmask == NULL) {
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
    instance->ops.mask(instance->ctx);
}