#include "tsc.h"

#include <stdbool.h>
#include <stddef.h>

#include "cpuid.h"
#include "acpi/acpi.h"
#include "lib/defs.h"

/**
 * The calculated TSC resolution
 */
uint64_t g_tsc_freq_hz = 0;

/**
 * Calculate the TSC resolution, we have two supported methods:
 * - using the cpuid
 * - using a stall function + rdtsc
 *
 * we always prefer using the cpuid if available, but we fallback on the stall if not
 */
static uint32_t calculate_tsc() {
    uint32_t a, b, c, d;

    // check if we have the time stamp counter cpuid, if we do we can
    // very easily calculate the frequency right away
    uint32_t maxleaf = __get_cpuid_max(0, NULL);
    if (maxleaf >= 0x15) {
        __cpuid(0x15, a, b, c, d);

        // check that we have the ratio and the hz
        if (b != 0 && c != 0) {
            TRACE("timer: TSC Calculated from CPUID");
            return c * (b / a);
        }
    }

    // we are going to estimate the tsc, and we will align
    // to 10MHz just for a more stable result
    TRACE("timer: TSC estimated using ACPI timer");

    // set the counter to FFs

    // start the timer
    uint64_t start_tsc = get_tsc();
    uint32_t ticks = acpi_get_timer_tick() + 363;
    while (((ticks - acpi_get_timer_tick()) & BIT23) == 0);
    uint64_t end_tsc = get_tsc();
    return (end_tsc - start_tsc) * 9861;
}

void init_tsc() {
    g_tsc_freq_hz = calculate_tsc();
    TRACE("timer: TSC frequency %luMHz", g_tsc_freq_hz / 1000000);
    ASSERT(g_tsc_freq_hz != 0);
}
