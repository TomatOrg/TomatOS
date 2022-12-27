#include "tick.h"
#include "tsc.h"
#include "thread/cpu_local.h"

static CPU_LOCAL uintptr_t m_base_tick = 0;

void sync_tick() {
    m_base_tick = get_tsc();
}

INTERRUPT int64_t get_tick() {
    return (get_tsc() - m_base_tick) / get_tsc_freq();
}

int64_t get_total_tick() {
    return get_tsc() / get_tsc_freq();
}

int64_t microtime() {
    return get_tick() / TICKS_PER_MICROSECOND;
}
