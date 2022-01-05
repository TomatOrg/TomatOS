#include "ticketlock.h"

#include <stdatomic.h>

void ticketlock_lock(ticketlock_t* lock) {
    size_t ticket = atomic_fetch_add_explicit(&lock->next_ticket, 1, memory_order_relaxed);

    while (atomic_load_explicit(&lock->next_serving, memory_order_acquire) != ticket) {
        __builtin_ia32_pause();
    }
}

void ticketlock_unlock(ticketlock_t* lock) {
    atomic_fetch_add_explicit(&lock->next_serving, 1, memory_order_release);
}

bool ticketlock_is_locked(ticketlock_t* lock) {
    size_t ticket = atomic_load_explicit(&lock->next_ticket, memory_order_relaxed);
    return atomic_load_explicit(&lock->next_serving, memory_order_relaxed) != ticket;
}
