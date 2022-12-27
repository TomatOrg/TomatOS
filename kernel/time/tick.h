#pragma once

#include <stdint.h>
#include "arch/idt.h"

#define NANOSECONDS_PER_TICK    100
#define TICKS_PER_MICROSECOND   (1000 / NANOSECONDS_PER_TICK)
#define TICKS_PER_MILLISECOND   (TICKS_PER_MICROSECOND * 1000)
#define TICKS_PER_SECOND        (TICKS_PER_MILLISECOND * 1000)

/**
 * Sync the tick count between different cores
 */
void sync_tick();

/**
 * Get a timer tick, it starts when the system starts, and will
 * always grow monotonically.
 */
INTERRUPT int64_t get_tick();

/**
 * Get the current tick without the base tick
 */
int64_t get_total_tick();

/**
 * Get the time in microsecond since boot
 */
int64_t microtime();
