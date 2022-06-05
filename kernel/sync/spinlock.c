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
    while (!spinlock_try_lock_weak(spinlock)) {
        while (spinlock_is_locked(spinlock)) {
            __builtin_ia32_pause();
        }
    }
}

bool spinlock_try_lock(spinlock_t* spinlock) {
    bool _false = false;
    bool result = atomic_compare_exchange_strong_explicit(&spinlock->locked, &_false, true,
                                                   memory_order_acquire, memory_order_relaxed);
    return result;
}

void spinlock_unlock(spinlock_t* spinlock) {
    atomic_store_explicit(&spinlock->locked, false, memory_order_release);
}

bool spinlock_is_locked(spinlock_t* spinlock) {
    return atomic_load_explicit(&spinlock->locked, memory_order_relaxed);
}
