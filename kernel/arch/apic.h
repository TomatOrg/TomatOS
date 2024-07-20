#pragma once

#include <stdint.h>

/**
 * Initialize the APIC per core
 */
void init_lapic_per_core(void);

/**
 * Request an EOI signal to be sent
 */
void lapic_eoi(void);

