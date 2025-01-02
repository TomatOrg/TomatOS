#include "tsc.h"

#include "arch/cpuid.h"
#include "debug/log.h"
#include "acpi/acpi.h"
#include "lib/defs.h"

#include <stddef.h>

/**
 * The calculated TSC resolution
 */
uint64_t g_tsc_resolution_hz = 0;

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
    if (maxleaf >= CPUID_TIME_STAMP_COUNTER) {
        __cpuid(CPUID_TIME_STAMP_COUNTER, 0, a, b, c, d);

        // check that we have the ratio and the hz
        if (b != 0 && c != 0) {
            LOG_DEBUG("timer: TSC Calculated from CPUID");
            return c * (b / c);
        }
    }

    // TODO: it seems that in theory on certain intel cpus we can
    //       use the platform info MSR to calculate the TSC speed

    // we are going to estimate the tsc, and we will align
    // to 10MHz just for a more stable result
    LOG_DEBUG("timer: TSC estimated using acpi_stall");
    uint64_t start = get_tsc();
    acpi_stall(US_PER_S);
    return ALIGN_MUL_NEAR(get_tsc() - start, 10000000);
}

void init_tsc() {
    g_tsc_resolution_hz = calculate_tsc();
    LOG_INFO("timer: TSC frequency %uMHz", g_tsc_resolution_hz / 1000000);
}

//
// Intel CPUs require a fence
//

uint64_t tsc_set_timeout(uint64_t timeout) {
    uint64_t deadline = get_tsc() + timeout;
    __wrmsr(MSR_IA32_TSC_DEADLINE, deadline);
    return deadline;
}

void tsc_disable_timeout(void) {
    __wrmsr(MSR_IA32_TSC_DEADLINE, 0);
}
