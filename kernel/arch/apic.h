#pragma once

#include "util/except.h"

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

/**
 * Configures the lapic to fire a wakeup
 */
void lapic_set_wakeup();

/**
 * Configures the lapic to fire a preempt
 */
void lapic_set_preempt();

/**
 * Set the deadline in ticks since now
 */
void lapic_set_timeout(uint64_t ticks);

/**
 * Set the exact deadline and not an offset
 */
void lapic_set_deadline(uint64_t ticks);
