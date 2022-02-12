#pragma once

#include "util/except.h"

/**
 * These are ordered by interrupt priority
 */
typedef enum irq {
    /**
     * Preempting IRQ, comes from a time slice
     */
    IRQ_PREEMPT     = 0xF0,

    /**
     * Schedule irq, will schedule a new thread, see the
     * scheduler on the difference from yield, comes from int
     * so no need for eoi
     */
    IRQ_SCHEDULE    = 0xF1,

    /**
     * Yield irq, will yield the current thread, comes
     * from a normal int so no need to eoi
     */
    IRQ_YIELD       = 0xF2,

    /**
     * Parks the current thread
     */
    IRQ_PARK        = 0xF3,

    /**
     * Startup the scheduler
     */
    IRQ_STARTUP     = 0xF4,

    /**
     * Spurious interrupt, have it the highest to
     * just ignore it as quickly as possible
     */
    IRQ_SPURIOUS    = 0xFF,
} irq_t;

/**
 * Get the apic base for early printing
 */
void early_init_apic();

/**
 * Initialize the apic, including mapping it and program
 * the virtual wires
 */
err_t init_apic();

/**
 * Send eoi to the lapic
 */
void lapic_eoi();

/**
 * Send an ipi to another core
 */
void lapic_send_ipi(uint8_t vector, int apic);

/**
 * Send an IPI to the lowest priority CPU
 *
 * @param vector    [IN] The vector to send
 */
void lapic_send_ipi_lowest_priority(uint8_t vector);

/**
 * Get the apic id of the current cpu
 */
size_t get_apic_id();
