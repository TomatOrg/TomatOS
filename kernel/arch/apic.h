#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <lib/except.h>

/**
 * Initialize the APIC globally
 */
err_t init_lapic(void);

/**
 * Initialize the APIC per core
 */
void init_lapic_per_core(void);

/**
 * Request an EOI signal to be sent
 */
void lapic_eoi(void);

void lapic_timer_set_deadline(uint64_t tsc_deadline);
void lapic_timer_clear(void);
