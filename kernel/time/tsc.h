#pragma once

#include <util/except.h>
#include <arch/idt.h>

/**
 * Initialize the relative time
 */
err_t init_tsc();

/**
 * Gets the TSC frequency in ticks
 */
uint64_t get_tsc_freq();

/**
 * Get the current TSC
 */
uint64_t get_tsc();
