#pragma once

#include <util/except.h>

#include <stdint.h>

/**
 * Initializes the delay code
 */
err_t init_delay();

/**
 * Count the amount of time
 *
 * @param delay_time    [IN] Number of microseconds to delay
 */
void microdelay(uint64_t delay_time);
