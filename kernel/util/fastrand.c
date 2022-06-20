#include "fastrand.h"

#include <thread/cpu_local.h>

/**
 * Used as a sort of seed, it is per cpu so we won't have
 * to use any atomics
 */
static uint64_t CPU_LOCAL m_fast_rand;

INTERRUPT uint32_t fastrand() {
    m_fast_rand += 0xa0761d6478bd642f;
    __uint128_t i = (__uint128_t)m_fast_rand * (m_fast_rand ^ 0xe7037ed1a0b428db);
    uint64_t hi = (uint64_t)(i >> 64);
    uint64_t lo = (uint64_t)i;
    return (uint32_t)(hi ^ lo);
}

INTERRUPT uint32_t fastrandn(uint32_t n) {
    return ((uint64_t)fastrand() * (uint64_t)n) >> 32;
}

INTERRUPT uint64_t fastrand64() {
    m_fast_rand += 0xa0761d6478bd642f;
    __uint128_t i = (__uint128_t)m_fast_rand * (m_fast_rand ^ 0xe7037ed1a0b428db);
    uint64_t hi = (uint64_t)(i >> 64);
    uint64_t lo = (uint64_t)i;
    return hi ^ lo;
}
