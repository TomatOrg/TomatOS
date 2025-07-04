#pragma once

#include <stdbool.h>
#include <stdint.h>

#define MS_PER_S 1000
#define US_PER_S 1000000
#define NS_PER_S 1000000000

/**
 * The TSC resolution
 */
extern uint64_t g_tsc_freq_hz;

/**
 * Initialize the timer subsystem, calculating the frequency of the TSC so it can be used for time keeping
 */
void init_tsc();

/**
 * Returns true if the CPU supports TSC deadline
 */
bool tsc_deadline_is_supported();

/**
 * Get the raw tsc value
 */
static inline uint64_t get_tsc() { return __builtin_ia32_rdtsc(); }

static inline uint64_t ns_to_tsc(uint64_t ns) { return (ns * g_tsc_freq_hz) / NS_PER_S; }
static inline uint64_t us_to_tsc(uint64_t us) { return (us * g_tsc_freq_hz) / US_PER_S; }
static inline uint64_t ms_to_tsc(uint64_t ms) { return (ms * g_tsc_freq_hz) / MS_PER_S; }

static inline uint64_t tsc_to_ns(uint64_t ns) { return (ns * NS_PER_S) / g_tsc_freq_hz; }
static inline uint64_t tsc_to_us(uint64_t us) { return (us * US_PER_S) / g_tsc_freq_hz; }
static inline uint64_t tsc_to_ms(uint64_t ms) { return (ms * MS_PER_S) / g_tsc_freq_hz; }

static inline uint64_t tsc_ns_deadline(uint64_t ns) { return get_tsc() + ns_to_tsc(ns); }
static inline uint64_t tsc_us_deadline(uint64_t us) { return get_tsc() + us_to_tsc(us); }
static inline uint64_t tsc_ms_deadline(uint64_t ms) { return get_tsc() + ms_to_tsc(ms); }

static inline bool tsc_check_deadline(uint64_t tsc) { return tsc <= get_tsc(); }

void tsc_timer_set_deadline(uint64_t tsc_deadline);
void tsc_timer_clear(void);