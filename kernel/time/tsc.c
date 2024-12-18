#include "tsc.h"

#include "arch/cpuid.h"
#include "debug/log.h"
#include "acpi/acpi.h"
#include "lib/defs.h"

#include <stddef.h>

/**
 * The calculated TSC resolution
 */
static uint64_t m_tsc_resolution_hz = 0;

uint64_t get_tsc_precise() {
    asm("lfence");
    return __builtin_ia32_rdtsc();
}

uint64_t get_tsc() {
    return __builtin_ia32_rdtsc();
}

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
        __cpuid(CPUID_TIME_STAMP_COUNTER, a, b, c, d);

        // check that we have the ratio and the hz
        if (b != 0 && c != 0) {
            LOG_DEBUG("timer: TSC Calculated from CPUID");
            return c * (b / c);
        }
    }

    // TODO: it seems that in theory on certain intel cpus we can
    //       use the platform info MSR to calculate the TSC speed

    LOG_DEBUG("timer: TSC estimated using acpi_stall");
    uint64_t start = get_tsc_precise();
    acpi_stall(100000);
    return ALIGN_MUL_NEAR(get_tsc_precise() - start, 1000000) * 10;
}

void init_tsc() {
    m_tsc_resolution_hz = calculate_tsc();
    LOG_INFO("timer: TSC frequency %uMHz", m_tsc_resolution_hz / 1000000);
}

uint64_t tsc_get_usecs() {
    return get_tsc_precise() / m_tsc_resolution_hz * 1000000;
}

void tsc_set_deadline(uint64_t deadline) {
    __wrmsr(MSR_IA32_TSC_DEADLINE, deadline);
}
