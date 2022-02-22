#pragma once

#include <util/except.h>

#define CPU_LOCAL __seg_gs __attribute__((section(".cpu_local")))

/**
 * Initialize the per-cpu storage
 */
err_t init_cpu_locals();

/**
 * Gets the absolute address of a per-cpu based variable
 *
 * @param ptr       [IN] per-cpu relative pointer
 */
void* get_cpu_local_base(__seg_gs void* ptr);

/**
 * Gets the absolute address of a per-cpu based variable
 *
 * @param cpu       [IN] The specific cpu to get the base for
 * @param ptr       [IN] per-cpu relative pointer
 */
void* get_cpu_base(int cpu, __seg_gs void* ptr);

/**
 * Get the id of the current cpu without
 * reading from the APIC
 */
int get_cpu_id();
