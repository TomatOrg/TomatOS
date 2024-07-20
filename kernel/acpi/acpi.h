#pragma once

#include <stdint.h>
#include "lib/except.h"

/**
 * Initialize the early acpi subsystem, should just be enough for
 * doing whatever we need to do
 */
err_t init_acpi();

/**
 * Stall for the given amount of NS
 */
void acpi_stall(uint64_t ns);
