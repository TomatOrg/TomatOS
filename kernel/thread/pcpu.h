#pragma once

#include "lib/except.h"

#define CPU_LOCAL _Thread_local

/**
 * Initialize the Per-cpu information for the current core
 */
err_t pcpu_init_per_core(int cpu_id);

/**
 * Gets the id of the current cpu
 */
int get_cpu_id();
