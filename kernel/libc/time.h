#pragma once
#include <stdint.h>
#include <time/tsc.h>
struct timespec {
    long tv_sec;
    long tv_nsec;
};
typedef enum { CLOCK_MONOTONIC = 0 } clockid_t;
static inline int clock_gettime(clockid_t clk_id, struct timespec *tp) {
    uint64_t time = microtime();
    tp->tv_sec = time / (1000*1000);
    tp->tv_nsec = (time % (1000*1000)) * 100;
    return 0;
}