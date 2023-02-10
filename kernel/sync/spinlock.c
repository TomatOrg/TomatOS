#include "spinlock.h"

#include <util/defs.h>

#include <stdatomic.h>
#include "arch/intrin.h"
#include "arch/idt.h"

INTERRUPT void spinlock_lock(spinlock_t* spinlock) {
    for (;;) {
        if (!atomic_exchange_explicit(&spinlock->lock, true, memory_order_acquire)) {
            return;
        }

        while (atomic_load_explicit(&spinlock->lock, memory_order_relaxed)) {
            __builtin_ia32_pause();
        }
    }
}

INTERRUPT bool spinlock_try_lock(spinlock_t* spinlock) {
    return !atomic_load_explicit(&spinlock->lock, memory_order_relaxed) &&
            atomic_exchange_explicit(&spinlock->lock, true, memory_order_acquire);
}

INTERRUPT void spinlock_unlock(spinlock_t* spinlock) {
    atomic_store_explicit(&spinlock->lock, false, memory_order_release);
}

INTERRUPT bool spinlock_is_locked(spinlock_t* spinlock) {
    return atomic_load_explicit(&spinlock->lock, memory_order_relaxed);
}

INTERRUPT static bool irq_save() {
    bool status = __readeflags() & BIT9;
    _disable();
    return status;
}

INTERRUPT static void irq_restore(bool status) {
    if (status) {
        _enable();
    }
}

INTERRUPT void irq_spinlock_lock(irq_spinlock_t* spinlock) {
    bool status = irq_save();
    spinlock_lock(&spinlock->lock);
    spinlock->status = status;
}

INTERRUPT bool irq_spinlock_try_lock(irq_spinlock_t* spinlock) {
    bool status = irq_save();
    bool success = spinlock_try_lock(&spinlock->lock);
    if (!success) {
        irq_restore(status);
    }
    return success;
}

INTERRUPT void irq_spinlock_unlock(irq_spinlock_t* spinlock) {
    bool status = spinlock->status;
    spinlock_unlock(&spinlock->lock);
    irq_restore(status);
}

INTERRUPT bool irq_spinlock_is_locked(irq_spinlock_t* spinlock) {
    return spinlock_is_locked(&spinlock->lock);
}
