#pragma once

#include <stdint.h>

#include <lib/except.h>

/**
 * Initialize the early acpi subsystem, should just be enough for
 * doing whatever we need to do
 */
err_t init_acpi_tables(void);

/**
 * Get the ACPI PM Timer tick value
 */
uint32_t acpi_get_timer_tick(void);

/**
 * Stall for the given amount of NS
 */
void acpi_stall(uint64_t ns);
