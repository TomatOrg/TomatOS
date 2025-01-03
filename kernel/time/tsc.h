#pragma once

#include <stdint.h>

#define MS_PER_S 1000
#define US_PER_S 1000000
#define NS_PER_S 1000000000

/**
 * The TSC resolution
 */
extern uint64_t g_tsc_resolution_hz;

/**
 * Initialize the timer subsystem, calculating the frequency of the TSC so it can be used for time keeping
 */
void init_tsc();

/**
 * Get the raw tsc value
 */
static inline uint64_t get_tsc() { return __builtin_ia32_rdtsc(); }

static inline uint64_t ns_to_tsc(uint64_t ns) { return (ns * g_tsc_resolution_hz) / NS_PER_S; }
static inline uint64_t us_to_tsc(uint64_t us) { return (us * g_tsc_resolution_hz) / US_PER_S; }
static inline uint64_t ms_to_tsc(uint64_t ms) { return (ms * g_tsc_resolution_hz) / MS_PER_S; }

static inline uint64_t tsc_get_ns() { return (get_tsc() * NS_PER_S) / g_tsc_resolution_hz; }
static inline uint64_t tsc_get_us() { return (get_tsc() * US_PER_S) / g_tsc_resolution_hz; }
static inline uint64_t tsc_get_ms() { return (get_tsc() * MS_PER_S) / g_tsc_resolution_hz; }

/**
 * Set a delay until the next tsc interrupt, returns the deadline
 */
uint64_t tsc_set_timeout(uint64_t delay);

/**
 * Disable the TSC deadline
 */
void tsc_disable_timeout(void);
