#include "spinlock.h"

#include <util/defs.h>

#include <stdatomic.h>
#include "arch/intrin.h"

static bool spinlock_try_lock_weak(spinlock_t* spinlock) {
    bool _false = false;
    return atomic_compare_exchange_weak_explicit(&spinlock->locked, &_false, true,
                                                 memory_order_acquire, memory_order_relaxed);
}

void spinlock_lock(spinlock_t* spinlock) {
    // read the interrupts status and disable interrupts
    bool status = (__readeflags() & BIT9) ? true : false;
    if (status) _disable();

    while (!spinlock_try_lock_weak(spinlock)) {
        // while spinning we can safely enable interrupts, just
        // need to disable interrupts before we are trying again
        if (status) _enable();
        while (spinlock_is_locked(spinlock)) {
            __builtin_ia32_pause();
        }
        if (status) _disable();
    }

    // success, save interrupt status
    spinlock->status = status;
}

bool spinlock_try_lock(spinlock_t* spinlock) {
    // read the interrupt status and disable interrupts
    bool status = (__readeflags() & BIT9) ? true : false;
    if (status) _disable();

    bool _false = false;
    bool result = atomic_compare_exchange_strong_explicit(&spinlock->locked, &_false, true,
                                                   memory_order_acquire, memory_order_relaxed);

    if (result) {
        // took the spinlock, save the interrupt status
        spinlock->status = status;
    } else {
        // failed to take the spinlock, enable interrupts
        if (status) _enable();
    }

    return status;
}

void spinlock_unlock(spinlock_t* spinlock) {
    atomic_store_explicit(&spinlock->locked, false, memory_order_release);

    // restore the interrupt status
    if (spinlock->status) {
        _enable();
    }
}

bool spinlock_is_locked(spinlock_t* spinlock) {
    return atomic_load_explicit(&spinlock->locked, memory_order_relaxed);
}
