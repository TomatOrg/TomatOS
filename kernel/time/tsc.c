#include "tsc.h"
#include "delay.h"

#include <arch/cpuid.h>

#include "arch/intrin.h"
#include "arch/idt.h"
#include "tick.h"

/**
 * The frequency of the cpu in ticks per millisecond
 */
static uint64_t m_tsc_freq = 0;

/**
 * Calibrates the TSC based timer, this happens when there are no
 * interrupts or scheduling so we are safe to use delay functions
 * to count the time
 */
static void calibrate_tsc() {
    // measure it, we will delay for one millisecond
    // to give enough headroom and accuracy
    int64_t begin_value = _rdtsc();
    _mm_lfence();
    microdelay(1000);
    int64_t end_value = _rdtsc();
    _mm_lfence();

    // and now calculate the tsc frequency
    m_tsc_freq = (end_value - begin_value) / TICKS_PER_MILLISECOND;
}

err_t init_tsc() {
    err_t err = NO_ERROR;

    // make sure we actually have a non-variant tsc
    cpuid_extended_time_stamp_counter_edx_t tsc_edx;
    cpuid(CPUID_EXTENDED_TIME_STAMP_COUNTER, NULL, NULL, NULL, &tsc_edx.packed);
    CHECK(tsc_edx.invariant_tsc);

    // calibrate the tsc
    calibrate_tsc();
    TRACE("TSC: %d tsc ticks per tick", m_tsc_freq);

cleanup:
    return err;
}

uint64_t get_tsc_freq() {
    return m_tsc_freq;
}

uint64_t get_tsc() {
    _mm_mfence();
    _mm_lfence();
    uint64_t value = _rdtsc();
    _mm_lfence();
    return value;
}
