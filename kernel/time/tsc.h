#pragma once

#include <stdint.h>

/**
 * Get precise timer cycles, using a proper barrier to make sure that the tsc is not speculated.
 * can be helpful for timekeeping
 */
uint64_t get_tsc_precise();

/**
 * Get the timer cycles without a fence, might get speculated
 */
uint64_t get_tsc();

/**
 * Initialize the timer subsystem, calculating the frequency of the TSC so it can be used for time keeping
 */
void init_tsc();

/**
 * Get the current time in microseconds
 */
uint64_t tsc_get_usecs();

/**
 * Set a deadline to when we want to trigger an interrupt
 */
void tsc_set_deadline(uint64_t deadline);
