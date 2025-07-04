#pragma once

#include <stdint.h>

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

/**
 * Set the timeout, in tsc ticks, until the next per-cpu interrupt
 */
void pcpu_timer_set_deadline(uint64_t tsc_deadline);

/**
 * Disable the per-cpu timer
 */
void pcpu_timer_clear(void);
