#pragma once


#include "thread/waitable.h"

typedef enum priority {
    /**
     * Normal running priority, allows the GC to be called
     */
    PRIORITY_NORMAL = 0x1,

    /**
     * Priority that does not have preemption, useful for cases where you don't
     * want to disable interrupts
     */
    PRIORITY_NO_PREEMPT = 0x2,
} priority_t;

/**
 * These are ordered by interrupt priority
 */
typedef enum irq {
    /**
     * Preempting IRQ, comes from a time slice preemption, this is
     * blocked by GC priority and above
     */
    IRQ_PREEMPT     = 0x20,

    /**
     * Wakeup the scheduler so it will run, does not actually run
     * anything, just used to wakeup the core from hlt.
     */
    IRQ_WAKEUP      = 0x30,

    // TODO: we need some space for legacy PIC irqs
    //       mostly for stuff like PS2

    /**
     * IRQ allocation range, used for MSI/MSI-x
     */
    IRQ_ALLOC_BASE = 0x40,
    IRQ_ALLOC_END = 0xEF,


    /**
     * Yield irq, will yield the current thread, comes
     * from a normal int so no need to eoi
     */
    IRQ_YIELD       = 0xF0,

    /**
     * Parks the current thread
     */
    IRQ_PARK        = 0xF1,

    /**
     * Startup the scheduler
     */
    IRQ_DROP     = 0xF2,

    /**
     * Spurious interrupt, have it the highest to
     * just ignore it as quickly as possible
     */
    IRQ_SPURIOUS    = 0xFF,
} irq_t;

typedef struct irq_handler {
    // Tells the irq subsystem how to mask the IRQ
    void (*mask)(void* ctx);

    // Tells the irq subsystem how to unmask the IRQ
    void (*unmask)(void* ctx);
} irq_ops_t;

err_t alloc_irq(int count, irq_ops_t* ops, void* ctx, uint8_t* vector);

void irq_wait(uint8_t handler, void* ctx);

void irq_dispatch(interrupt_context_t* ctx);
