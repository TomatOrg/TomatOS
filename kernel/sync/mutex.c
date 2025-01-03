#include "mutex.h"

#include "spin_wait.h"

/**
 * Used to indicate that the target thread should attempt to
 * lock the mutex again as soon as it is unparked
 */
#define TOKEN_NORMAL (0)

/**
 * Used to indicate that the mutex is being handed off to the target
 * thread directly without unlocking it
 */
#define TOKEN_HANDOFF (1)

static bool mutex_park_validate(void* arg) {
    mutex_t* mutex = arg;
    return atomic_load_explicit(&mutex->state, memory_order_relaxed) == (MUTEX_LOCKED | MUTEX_PARKED);
}

static void mutex_park_before_sleep(void* arg) {}

static void mutex_park_timed_out(void* arg, size_t key, bool was_last_thread) {
    // Clear the parked bit if we were the last parked thread
    mutex_t* mutex = arg;
    if (was_last_thread) {
        atomic_fetch_and_explicit(&mutex->state, ~MUTEX_PARKED, memory_order_relaxed);
    }
}

__attribute__((cold))
bool mutex_lock_slow(mutex_t* mutex, uint64_t ns_deadline) {
    spin_wait_t spin_wait = {};
    uint8_t state = atomic_load_explicit(&mutex->state, memory_order_relaxed);
    for (;;) {
        // Grab the lock if it isn't locked, even if there is a queue on it
        if ((state & MUTEX_LOCKED) == 0) {
            if (atomic_compare_exchange_weak_explicit(
                &mutex->state,
                &state, state | MUTEX_LOCKED,
                memory_order_acquire, memory_order_relaxed
            )) {
                return true;
            }

            continue;
        }

        // If there is no queue, try spinning a few times
        if ((state & MUTEX_PARKED) == 0 && spin_wait_spin(&spin_wait)) {
            state = atomic_load_explicit(&mutex->state, memory_order_relaxed);
            continue;
        }

        // Set the parked bit
        if ((state & MUTEX_PARKED) == 0) {
            if (!atomic_compare_exchange_weak_explicit(
                &mutex->state,
                &state, state | MUTEX_PARKED,
                memory_order_relaxed, memory_order_relaxed
            )) {
                continue;
            }
        }

        // Park our thread until we are woken up by an unlock
        park_result_t result = parking_lot_park(
            (size_t)mutex,
            mutex_park_validate,
            mutex_park_before_sleep,
            mutex_park_timed_out,
            mutex,
            0,
            ns_deadline
        );

        if (result.timed_out) {
            // timeout expired
            return false;

        } else if (result.invalid) {
            // The validation function failed, try locking again

        } else if (result.unpark_token == TOKEN_HANDOFF) {
            // The thread that unparked us passed the lock on to is
            // directly without unlocking it
            return true;

        } else {
            // We were unparked normally, try acquiring the lock again
        }

        // Loop back and try locking again
        spin_wait_reset(&spin_wait);
        state = atomic_load_explicit(&mutex->state, memory_order_relaxed);
    }
}

static size_t mutex_unpark_callback(void* arg, unpark_result_t result) {
    mutex_t* mutex = arg;

    // If we are using a fair unlock then we should keep the
    // mutex locked and hand it off to the unparked thread.
    if (result.unparked_threads != 0 && result.be_fair) {
        // Clear the parked bit if there are no more parked
        // threads.
        if (!result.have_more_threads) {
            atomic_store_explicit(&mutex->state, MUTEX_LOCKED, memory_order_relaxed);
        }
        return TOKEN_HANDOFF;
    }

    // Clear the locked bit, and the parked bit as well if there
    // are no more parked threads.
    if (result.have_more_threads) {
        atomic_store_explicit(&mutex->state, MUTEX_PARKED, memory_order_relaxed);
    } else {
        atomic_store_explicit(&mutex->state, 0, memory_order_relaxed);
    }

    return TOKEN_NORMAL;
}

__attribute__((cold))
void mutex_unlock_slow(mutex_t* mutex) {
    size_t addr = (size_t)mutex;
    parking_lot_unpark_one(addr, mutex_unpark_callback, mutex);
}
