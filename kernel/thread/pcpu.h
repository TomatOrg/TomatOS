#pragma once

#include "lib/except.h"

#define CPU_LOCAL _Thread_local

/**
 * Perform the main allocation
 */
err_t pcpu_init(int cpu_count);

/**
 * Initialize the Per-cpu information for the current core
 */
void pcpu_init_per_core(int cpu_id);

/**
 * Gets the id of the current cpu
 */
int get_cpu_id();
