#pragma once

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

