#include "irq.h"
#include "dotnet/gc/heap.h"
#include "thread/scheduler.h"
#include "arch/intrin.h"

typedef struct irq_instance {
    // The thread waiting on this irq, NULL if none
    thread_t* waiting_thread;

    // lock to protect the irq instance
    spinlock_t lock;

    // we got a trigger, so next time someone waits
    // we will not let them
    bool triggered;

    // is this entry allocated
    bool allocated;
} irq_instance_t;

// TODO: per-cpu irq

// The irq entries
static irq_instance_t m_irqs[IRQ_ALLOC_END - IRQ_ALLOC_BASE] = { 0 };

// Sync the subsystem
static spinlock_t m_irq_alloc_lock = INIT_SPINLOCK();

err_t alloc_irq(int count, uint8_t* vector) {
    err_t err = NO_ERROR;

    spinlock_lock(&m_irq_alloc_lock);

    int found = 0;
    int entry = 0;
    for (entry = 0; entry < ARRAY_LEN(m_irqs); entry++) {
        if (m_irqs[entry].allocated) {
            found = 0;
        } else {
            found++;
        }

        if (found == count) break;
    }
    // make sure we actually found an entry
    CHECK(found == count);

    entry -= count - 1;
    // go backwards and set all the entries
    for (int i = 0; i < count; i++) {
        m_irqs[entry + i].allocated = true;
    }
    // set the base
    *vector = entry + IRQ_ALLOC_BASE;

cleanup:
    spinlock_unlock(&m_irq_alloc_lock);
    
    return err;
}

void irq_wait(uint8_t handler) {
    // get the handler
    ASSERT(handler >= IRQ_ALLOC_BASE);
    uint8_t idx = handler - IRQ_ALLOC_BASE;
    ASSERT(idx < ARRAY_LEN(m_irqs));
    ASSERT(m_irqs[idx].allocated);
    irq_instance_t* instance = &m_irqs[idx];

    irq_disable();
    spinlock_lock(&instance->lock);

    if (!instance->triggered) {
        // set the current thread as waiting for it
        ASSERT(instance->waiting_thread == NULL);
        instance->waiting_thread = get_current_thread();

        // park the current thread, will wake it up later. makes sure to release
        // the irq lock
        scheduler_park((void*)spinlock_unlock, &instance->lock);
    } else {
        // we already got a trigger, clean that up
        instance->triggered = false;
        spinlock_unlock(&instance->lock);
    }
    irq_enable();
}

INTERRUPT void irq_dispatch(interrupt_context_t* ctx) {
    // get the handler
    ASSERT(ctx->int_num >= IRQ_ALLOC_BASE);
    uint8_t idx = ctx->int_num - IRQ_ALLOC_BASE;
    ASSERT(idx < ARRAY_LEN(m_irqs));
    ASSERT(m_irqs[idx].allocated);
    irq_instance_t* instance = &m_irqs[idx];

    spinlock_lock(&instance->lock);

    if (!instance->allocated) {
        WARN("irq: IRQ #%d: no one is handling this irq", ctx->int_num);
    } else {
        // make sure we have a waiting thread
        if (instance->waiting_thread == NULL) {
            // mark as triggered
            instance->triggered = true;
        } else {
            // no one is waiting on this anymore
            thread_t* thread = instance->waiting_thread;
            instance->waiting_thread = NULL;

            // schedule the thread right now
            scheduler_ready_thread(thread);
        }
    }

    spinlock_unlock(&instance->lock);
}

INTERRUPT bool irq_save() {
    bool status = __readeflags() & BIT9;
    _disable();
    return status;
}

INTERRUPT void irq_restore(bool status) {
    if (status) {
        _enable();
    }
}

INTERRUPT void irq_enable() {
    ASSERT(!(__readeflags() & BIT9));
    _enable();
}

INTERRUPT void irq_disable() {
    ASSERT(__readeflags() & BIT9);
    _disable();
}

