#pragma once

#include "semaphore.h"

#include <stdint.h>

typedef struct mutex {
    _Atomic(int32_t) state;
    semaphore_t semaphore;
} mutex_t;

#define INIT_MUTEX() ((mutex_t){})

void mutex_lock(mutex_t* mutex);

bool mutex_try_lock(mutex_t* mutex);

void mutex_unlock(mutex_t* mutex);

void mutex_self_test();
