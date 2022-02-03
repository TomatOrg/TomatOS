#include "rwlock.h"

#include <stdatomic.h>

#define READER (1 << 1)
#define WRITER 1

void rwlock_read(rwlock_t* rwlock) {
    while (!rwlock_try_read(rwlock)) {
        __builtin_ia32_pause();
    }
}

static bool rwlock_try_write_weak(rwlock_t* rwlock) {
    size_t value = 0;
    return atomic_compare_exchange_weak_explicit(&rwlock->lock, &value, WRITER, memory_order_acquire, memory_order_relaxed);
}

void rwlock_write(rwlock_t* rwlock) {
    while (!rwlock_try_write_weak(rwlock)) {
        __builtin_ia32_pause();
    }
}

bool rwlock_try_read(rwlock_t* rwlock) {
    size_t value = atomic_fetch_add_explicit(&rwlock->lock, READER, memory_order_acquire);

    if ((value & WRITER) != 0) {
        // lock is taken, undo
        atomic_fetch_sub_explicit(&rwlock->lock, READER, memory_order_release);
        return false;
    }

    return true;
}

bool rwlock_try_write(rwlock_t* rwlock) {
    size_t value = 0;
    return atomic_compare_exchange_strong_explicit(&rwlock->lock, &value, WRITER, memory_order_acquire, memory_order_relaxed);
}

void rwlock_read_unlock(rwlock_t* rwlock) {
    atomic_fetch_sub_explicit(&rwlock->lock, READER, memory_order_release);
}

void rwlock_write_unlock(rwlock_t* rwlock) {
    atomic_fetch_and_explicit(&rwlock->lock, ~WRITER, memory_order_release);
}

