#pragma once

#include <stdint.h>
#include <arch/intrin.h>
#include <thread/scheduler.h>

typedef struct spin_wait {
    uint32_t counter;
} spin_wait_t;

static inline void spin_wait_reset(spin_wait_t* spin) {
    spin->counter = 0;
}

static inline bool spin_wait_spin(spin_wait_t* spin) {
    if (spin->counter >= 10) {
        return false;
    }

    spin->counter++;

    if (spin->counter <= 3) {
        uint32_t iterations = 1 << spin->counter;
        for (int i = 0; i < iterations; i++) {
            cpu_relax();
        }
    } else {
        scheduler_yield();
    }

    return true;
}

static inline void spin_wait_spin_no_yield(spin_wait_t* spin) {
    spin->counter++;
    if (spin->counter > 10) {
        spin->counter = 10;
    }

    uint32_t iterations = 1 << spin->counter;
    for (int i = 0; i < iterations; i++) {
        cpu_relax();
    }
}
