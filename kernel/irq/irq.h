#pragma once

#include <util/except.h>
#include <arch/idt.h>

#include <stdint.h>

typedef enum priority {
    /**
     * Lowest priority available, used so scheduler waiting for interrupts
     * has priority over active cpus to get an irq
     */
    PRIORITY_SCHEDULER_WAIT = 0x0,

    /**
     * Normal running priority, allows to preempt the thread
     */
    PRIORITY_NORMAL = 0x1,

    /**
     * Priority that does not have preemption, useful for cases
     * where you don't want to disable interrupts (for example to
     * create gc safe areas)
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

    /**
     * IRQ allocation range, used for LAPIC IRQs like MSI(X) and IOAPIC ones
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
     * Creates a trace of all the regs
     * and the stack at the calling point
     */
    IRQ_TRACE    = 0xF3,

    /**
     * Spurious interrupt, have it the highest to
     * just ignore it as quickly as possible
     */
    IRQ_SPURIOUS    = 0xFF,
} irq_t;

typedef struct irq_ops {
    // Tells the irq subsystem how to mask the IRQ
    void (*mask)(void* ctx);

    // Tells the irq subsystem how to unmask the IRQ
    void (*unmask)(void* ctx);
} irq_ops_t;

/**
 * Allocate a new IRQ for the device
 *
 * @param count     [IN]    How many irqs to allocate, sequentially
 * @param ops       [IN]    The IRQ operations needed from the driver
 * @param ctx       [IN]    The context to pass to the operations
 * @param vector    [OUT]   The allocated base vector
 */
err_t alloc_irq(int count, irq_ops_t ops, void* ctx, uint8_t* vector);

/**
 * Wait for the given IRQ, passing in the context for the specific one
 * @param vector    [IN]    The irq we are waiting on
 */
void irq_wait(uint8_t vector);

/**
 * Dispatches an IRQ, note that this may create a new thread and set a
 * new thread context.
 *
 * @param ctx       [IN] The interrupt context of the current IRQ
 */
void irq_dispatch(interrupt_context_t* ctx);
