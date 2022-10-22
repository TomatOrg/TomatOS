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

#define NOINSTRUMENT __attribute__((no_instrument_function))

static __thread bool m_instrument_enable = false;

#define LOG_BUFFER_SIZE (64 * 1024 * 1024)
static uint32_t m_log_buffer[LOG_BUFFER_SIZE];
static int m_log_buffer_idx;


void profiler_start() {
    TRACE("Profiler started");
    m_log_buffer_idx = 0;
    m_instrument_enable = true;
}
void profiler_stop() {
    m_instrument_enable = false;
    TRACE("Profiler finished: memsave 0x%p %d profiler.trace", m_log_buffer, m_log_buffer_idx * 4);
}

INTERRUPT NOINSTRUMENT
void __cyg_profile_func_enter(void* func, void* call) {
    if (!m_instrument_enable) return;
    m_log_buffer[m_log_buffer_idx++] = ((uint32_t)(uintptr_t)func); // | (1UL << 31) // not needed for tomatos, but if we ever use this in userspace it is
    m_log_buffer[m_log_buffer_idx++] = _rdtsc() & ~(1UL << 31);
    if (m_log_buffer_idx >= LOG_BUFFER_SIZE) profiler_stop();
}

INTERRUPT NOINSTRUMENT
void __cyg_profile_func_exit(void* func, void* call) {
    if (!m_instrument_enable) return;
    m_log_buffer[m_log_buffer_idx++] = _rdtsc() & ~(1UL << 31);
}
