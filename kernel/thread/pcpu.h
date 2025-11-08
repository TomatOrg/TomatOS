#pragma once

#include <stdint.h>

#include "lib/except.h"

/**
 * We are going to use the FS segment for cpu-local storage
 */
#define CPU_LOCAL __attribute__((section("pcpu_data"), address_space(257)))

void* pcpu_get_pointer(__seg_fs void* ptr);

void* pcpu_get_pointer_of(__seg_fs void* ptr, int cpu_id);

/**
 * Early init pcpu data on the BSP, to allow us
 * to access PCPU data right away
 */
void init_early_pcpu(void);

/**
 * Perform the main allocation
 */
err_t init_pcpu(int cpu_count);

/**
 * Initialize the Per-cpu information for the current core
 */
err_t pcpu_init_per_core(int cpu_id);

/**
 * Gets the id of the current cpu
 */
int get_cpu_id();
