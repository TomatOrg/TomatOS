#pragma once

#include <stdint.h>

struct timeval {
    uint64_t tv_sec;
    uint64_t tv_usec;
};

struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

/**
 * Dummy implementation, doesn't actually return real time but it is good enough for MIR
 * which uses it for measuring time
 */
int gettimeofday(struct timeval *restrict tv, struct timezone *restrict tz);
