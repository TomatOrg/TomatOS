#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <lib/defs.h>

#define MUTEX_LOCKED    BIT0
#define MUTEX_PARKED    BIT1

typedef struct mutex {
    _Atomic(uint8_t) state;
} mutex_t;

bool mutex_lock_slow(mutex_t* mutex, uint64_t deadline);
void mutex_unlock_slow(mutex_t* mutex);

static inline void mutex_lock(mutex_t* mutex) {
    uint8_t zero;
    if (!atomic_compare_exchange_weak_explicit(
        &mutex->state,
        &zero, MUTEX_LOCKED,
        memory_order_acquire, memory_order_relaxed
    )) {
        mutex_lock_slow(mutex, 0);
    }
}

static inline bool mutex_try_lock(mutex_t* mutex) {
    uint8_t state = atomic_load_explicit(&mutex->state, memory_order_relaxed);
    for (;;) {
        if ((state & MUTEX_LOCKED) != 0) {
            return false;
        }

        if (atomic_compare_exchange_weak_explicit(
            &mutex->state,
            &state, state | MUTEX_LOCKED,
            memory_order_acquire, memory_order_relaxed
        )) {
            return true;
        }
    }
}

static inline bool mutex_is_locked(mutex_t* mutex) {
    uint8_t state = atomic_load_explicit(&mutex->state, memory_order_relaxed);
    return (state & MUTEX_LOCKED) != 0;
}

static inline void mutex_unlock(mutex_t* mutex) {
    uint8_t locked = MUTEX_LOCKED;
    if (atomic_compare_exchange_strong_explicit(
        &mutex->state,
        &locked, 0,
        memory_order_release, memory_order_relaxed
    )) {
        return;
    }

    mutex_unlock_slow(mutex);
}

// TODO: lock until/for
