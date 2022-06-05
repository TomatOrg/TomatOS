#pragma once

#include <stdbool.h>

typedef struct irq_spinlock {
    _Atomic(bool) locked;
    bool status;
} irq_spinlock_t;

#define INIT_IRQ_SPINLOCK() ((irq_spinlock_t){ .locked = false, .status = false })

void irq_spinlock_lock(irq_spinlock_t* spinlock);

bool irq_spinlock_try_lock(irq_spinlock_t* spinlock);

void irq_spinlock_unlock(irq_spinlock_t* spinlock);

bool irq_spinlock_is_locked(irq_spinlock_t* spinlock);
