
#include "arch/intrin.h"

#include "spinlock.h"
#include "lib/defs.h"

void spinlock_lock(spinlock_t* spinlock) {
    for (;;) {
        if (!atomic_exchange_explicit(&spinlock->lock, true, memory_order_acquire)) {
            return;
        }

        while (atomic_load_explicit(&spinlock->lock, memory_order_relaxed)) {
            cpu_relax();
        }
    }
}

bool spinlock_try_lock(spinlock_t* spinlock) {
    return !atomic_load_explicit(&spinlock->lock, memory_order_relaxed) &&
            atomic_exchange_explicit(&spinlock->lock, true, memory_order_acquire);
}

void spinlock_unlock(spinlock_t* spinlock) {
    atomic_store_explicit(&spinlock->lock, false, memory_order_release);
}

bool spinlock_is_locked(spinlock_t* spinlock) {
    return atomic_load_explicit(&spinlock->lock, memory_order_relaxed);
}

bool irq_spinlock_lock(irq_spinlock_t* spinlock) {
    bool state = irq_save();
    spinlock_lock(&spinlock->lock);
    return state;
}

void irq_spinlock_unlock(irq_spinlock_t* spinlock, bool irq_state) {
    spinlock_unlock(&spinlock->lock);
    irq_restore(irq_state);
}


bool irq_save() {
    bool status = __builtin_ia32_readeflags_u64() & BIT9;
    asm("cli");
    return status;
}

void irq_restore(bool status) {
    if (status) {
        asm("sti");
    }
}
