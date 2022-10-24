#include <stddef.h>
#include <stdint.h>
#include <util/printf.h>
#include <arch/idt.h>
#include <mem/phys.h>
#include <stdbool.h>
#include <arch/intrin.h>
#include <stdatomic.h>
#include <string.h>
#include <debug/debug.h>
#include <thread/scheduler.h>
#include <time/tsc.h>

#define NOINSTRUMENT __attribute__((no_instrument_function))

#ifdef PROF
static __thread bool m_instrument_enable = false;

#define LOG_BUFFER_SIZE (64 * 1024 * 1024)
static uint64_t m_log_buffer[LOG_BUFFER_SIZE];
static int m_log_buffer_idx;
#endif

void profiler_start() {
#ifdef PROF
    TRACE("Profiler started");
    m_log_buffer_idx = 0;
    m_log_buffer[m_log_buffer_idx++] = get_tsc_freq();
    m_instrument_enable = true;
#endif
}
void profiler_stop() {
#ifdef PROF
    m_instrument_enable = false;
    TRACE("Profiler finished: memsave 0x%p %d profiler.trace", m_log_buffer, m_log_buffer_idx * 8);
#endif
}

#ifdef PROF
INTERRUPT NOINSTRUMENT
void __cyg_profile_func_enter(void* func, void* call) {
    if (!m_instrument_enable) return;
    m_log_buffer[m_log_buffer_idx++] = (uintptr_t)func;
    m_log_buffer[m_log_buffer_idx++] = _rdtsc();
    if (m_log_buffer_idx >= LOG_BUFFER_SIZE) profiler_stop();
}

INTERRUPT NOINSTRUMENT
void __cyg_profile_func_exit(void* func, void* call) {
    if (!m_instrument_enable) return;
    m_log_buffer[m_log_buffer_idx++] = _rdtsc();
}
#endif