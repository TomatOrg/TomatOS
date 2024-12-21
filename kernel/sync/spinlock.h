#pragma once

#include <stdatomic.h>
#include <stdbool.h>

typedef struct spinlock {
    atomic_bool lock;
} spinlock_t;

#define INIT_SPINLOCK() ((spinlock_t){ .lock = false })

void spinlock_lock(spinlock_t* spinlock);

bool spinlock_try_lock(spinlock_t* spinlock);

void spinlock_unlock(spinlock_t* spinlock);

bool spinlock_is_locked(spinlock_t* spinlock);

bool irq_save();

void irq_restore(bool status);
