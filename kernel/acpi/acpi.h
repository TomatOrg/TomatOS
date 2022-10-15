#pragma once

#include "util/except.h"

typedef struct acpi_common_header {
    uint32_t signature;
    uint32_t length;
} PACKED acpi_common_header_t;

typedef struct acpi_descriptor_header {
    uint32_t signature;
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    uint8_t oem_id[6];
    uint64_t oem_table_id;
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} PACKED acpi_descriptor_header_t;

/**
 * Pointer to the RSDT, for use in C#
 */
extern void* g_rsdp;

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