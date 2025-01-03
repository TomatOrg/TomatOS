#pragma once
#include "mutex.h"

typedef struct condvar {
    _Atomic(mutex_t*) mutex;
} condvar_t;

#define INIT_CONDVAR() ((condvar_t){ .mutex = NULL })

bool condvar_notify_one_slow(condvar_t* condvar, mutex_t* mutex);
size_t condvar_notify_all_slow(condvar_t* condvar, mutex_t* mutex);
bool condvar_wait_until(condvar_t* condvar, mutex_t* mutex, uint64_t deadline);

static inline bool condvar_notify_one(condvar_t* condvar) {
    mutex_t* mutex = atomic_load_explicit(&condvar->mutex, memory_order_relaxed);
    if (mutex == NULL) {
        return false;
    }
    return condvar_notify_one_slow(condvar, mutex);
}

static inline size_t condvar_notify_all(condvar_t* condvar) {
    mutex_t* mutex = atomic_load_explicit(&condvar->mutex, memory_order_relaxed);
    if (mutex == NULL) {
        return 0;
    }
    return condvar_notify_all_slow(condvar, mutex);
}
