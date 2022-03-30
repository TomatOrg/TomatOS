#pragma once

#include <stdbool.h>

typedef struct spinlock {
    _Atomic(bool) locked;
    bool status;
} spinlock_t;

#define INIT_SPINLOCK() ((spinlock_t){ .locked = false, .status = false })

void spinlock_lock(spinlock_t* spinlock);

bool spinlock_try_lock(spinlock_t* spinlock);

void spinlock_unlock(spinlock_t* spinlock);

bool spinlock_is_locked(spinlock_t* spinlock);
