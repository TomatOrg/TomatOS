#include "time.h"
#include "libc/assert.h"
#include "time/tsc.h"
#include "time/tick.h"

int gettimeofday(struct timeval *restrict tv, struct timezone *restrict tz) {
    ASSERT(tz == NULL);
    uint64_t utime = NANOSECONDS_PER_TICK * get_tick();
    tv->tv_usec = utime % 1000000;
    tv->tv_sec = utime / 1000000;
    return 0;
}