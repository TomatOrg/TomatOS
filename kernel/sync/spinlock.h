#pragma once

#include <stdatomic.h>
#include <stdbool.h>

#include "arch/intrin.h"
#include "lib/defs.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Simple spinlock
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct spinlock {
    atomic_flag lock;
} spinlock_t;

#define SPINLOCK_INIT ((spinlock_t){ .lock = ATOMIC_FLAG_INIT })

static inline void spinlock_acquire(spinlock_t* lock) {
    while (atomic_flag_test_and_set_explicit(&lock->lock, memory_order_acquire)) {
        cpu_relax();
    }
}

static inline void spinlock_release(spinlock_t* lock) {
    atomic_flag_clear_explicit(&lock->lock, memory_order_release);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IRQ enable/disable helpers
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static inline void irq_enable() { asm("sti"); }
static inline void irq_disable() { asm("cli"); }
static inline bool is_irq_enabled() { return __builtin_ia32_readeflags_u64() & BIT9; }

static inline bool irq_save() {
    bool status = is_irq_enabled();
    irq_disable();
    return status;
}

static inline void irq_restore(bool irq_status) {
    if (irq_status) {
        irq_enable();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Spinlock shared between in-irq and out-of-irq code
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct irq_spinlock {
    atomic_flag lock;
} irq_spinlock_t;

#define IRQ_SPINLOCK_INIT ((irq_spinlock_t){ .lock = ATOMIC_FLAG_INIT })

static inline bool irq_spinlock_acquire(irq_spinlock_t* lock) {
    bool irq_state = irq_save();
    while (atomic_flag_test_and_set_explicit(&lock->lock, memory_order_acquire)) {
        cpu_relax();
    }
    return irq_state;
}

static inline void irq_spinlock_release(irq_spinlock_t* lock, bool irq_state) {
    atomic_flag_clear_explicit(&lock->lock, memory_order_release);
    irq_restore(irq_state);
}
