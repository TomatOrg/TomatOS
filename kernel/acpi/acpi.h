#pragma once

#include "util/except.h"

/**
 * Pointer to the RSDT, for use in C#
 */
extern struct acpi_descriptor_header* m_rsdt;


/**
 * Fetches all the tables that we need from ACPI for the kernel itself
 *
 * @remark
 * This does not actually enter ACPI mode or anything as advanced, it just parses the ACPI
 * tables so we can access them later and so we can initialize some of the basic services
 * needed by the kernel runtime (for example calibration of timers)
 */
err_t init_acpi();

/**
 * Get a certain acpi table
 *
 * @param signature [IN] The signature of the table
 */
void* acpi_get_table(uint32_t signature);