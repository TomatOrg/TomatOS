#pragma once

#include <util/except.h>

#define MICROSECONDS_PER_SECOND 1000000

/**
 * Initialize the relative time
 */
err_t init_rsc();

/**
 * Gets the TSC frequency
 */
uint64_t get_tsc_freq();

/**
 * Get a timer in microseconds, it has no defined start date, but will
 * always grow upward in microseconds
 */
uint64_t microtime();
