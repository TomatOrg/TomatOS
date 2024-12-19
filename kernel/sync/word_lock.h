#pragma once

#include <stdatomic.h>
#include <stddef.h>
#include <debug/log.h>
#include <lib/defs.h>

#define WORD_LOCK_LOCKED            1
#define WORD_LOCK_QUEUE_LOCKED      2
#define WORD_LOCK_QUEUE_MASK        (~3ull)

typedef struct word_lock {
    _Atomic(size_t) state;
} word_lock_t;

void word_lock_lock_slow(word_lock_t* lock);
void word_lock_unlock_slow(word_lock_t* lock);

static inline void word_lock_lock(word_lock_t* lock) {
    size_t zero = 0;
    if (atomic_compare_exchange_weak_explicit(
        &lock->state,
        &zero, WORD_LOCK_LOCKED,
        memory_order_acquire, memory_order_relaxed
    )) {
        return;
    }
    word_lock_lock_slow(lock);
}

static inline void word_lock_unlock(word_lock_t* lock) {
    uint8_t state = atomic_fetch_sub_explicit(&lock->state, WORD_LOCK_LOCKED, memory_order_release);
    if ((state & WORD_LOCK_QUEUE_LOCKED) || ((state & WORD_LOCK_QUEUE_MASK) == 0)) {
        return;
    }

    word_lock_unlock_slow(lock);
}
