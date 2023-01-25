/*
 * CODE TAKEN FROM WebKit WTF library
 *
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "mutex.h"
#include "thread/scheduler.h"

#define IS_HELD     1
#define HAS_PARKED  2
#define MASK        (IS_HELD | HAS_PARKED)

#define TOKEN_DIRECT_HANDOFF        1
#define TOKEN_BARGING_OPPORTUNITY    2

// This magic number turns out to be optimal based on past JikesRVM experiments.
#define SPIN_LIMIT 40

typedef struct compare_and_park_arg {
    _Atomic(uint8_t)* address;
    uint8_t expected;
} compare_and_park_arg_t;

static bool compare_and_park(compare_and_park_arg_t* arg) {
    uint8_t value = atomic_load(arg->address);
    return value == arg->expected;
}

bool mutex_is_locked(mutex_t* mutex) {
    return atomic_load_explicit(&mutex->byte, memory_order_acquire) & IS_HELD;
}

static void mutex_lock_slow(mutex_t* mutex) {
    unsigned spin_count = 0;

    for (;;) {
        uint8_t current_value = atomic_load(&mutex->byte);

        // We allow ourselves to barge in.
        if (!(current_value & IS_HELD)) {
            uint8_t temp = current_value;
            if (atomic_compare_exchange_weak(&mutex->byte, &temp, current_value | IS_HELD))
                return;
            continue;
        }

        // If there is nobody parked and we haven't spun too much, we can just try to spin around.
        if (!(current_value & HAS_PARKED) && (spin_count < SPIN_LIMIT)) {
            spin_count++;
            scheduler_yield();
            continue;
        }

        // Need to park. We do this by setting the parked bit first, and then parking. We spin around
        // if the parked bit wasn't set and we failed at setting it.
        if (!(current_value & HAS_PARKED)) {
            uint8_t new_value = current_value | HAS_PARKED;
            uint8_t temp = current_value;
            if (!atomic_compare_exchange_weak(&mutex->byte, &temp, new_value))
                continue;
            current_value = new_value;
        }

        ASSERT(current_value & IS_HELD);
        ASSERT(current_value & HAS_PARKED);

        // We now expect the value to be IS_HELD|HAS_PARKED. So long as that's the case, we can park.
        compare_and_park_arg_t arg = {
            .address = &mutex->byte,
            .expected = current_value
        };
        park_result_t result = park_conditionally(
            mutex,
            (park_validation_t)compare_and_park,
            NULL,
            &arg,
            -1
        );
        if (result.was_unparked) {
            switch (result.token) {
                case TOKEN_DIRECT_HANDOFF:
                    // The lock was never released. It was handed to us directly by the thread that did
                    // unlock(). This means we're done!
                    ASSERT(mutex_is_locked(mutex));
                    return;

                case TOKEN_BARGING_OPPORTUNITY:
                    // This is the common case. The thread that called unlock() has released the lock,
                    // and we have been woken up so that we may get an opportunity to grab the lock. But
                    // other threads may barge, so the best that we can do is loop around and try again.
                    break;
            }
        }

        // We have awoken, or we never parked because the byte value changed. Either way, we loop
        // around and try again.
    }
}

static bool mutex_lock_fast_assuming_zero(mutex_t* mutex) {
    uint8_t zero = 0;
    return atomic_compare_exchange_weak_explicit(&mutex->byte, &zero, IS_HELD, memory_order_acquire, memory_order_acquire);
}

void mutex_lock(mutex_t* mutex) {
    if (UNLIKELY(!mutex_lock_fast_assuming_zero(mutex))) {
        mutex_lock_slow(mutex);
    }
}

static intptr_t mutex_unpark_callback(unpark_result_t result, mutex_t* mutex) {
    // We are the only ones that can clear either the isHeldBit or the hasParkedBit,
    // so we should still see both bits set right now.
    ASSERT(atomic_load(&mutex->byte) == (IS_HELD | HAS_PARKED));

    if (result.did_unpark_thread && result.time_to_be_fair) {
        // We don't unlock anything. Instead, we hand the lock to the thread that was
        // waiting.
        return TOKEN_DIRECT_HANDOFF;
    }

    // we don't need anything more than a simple store in here, it will take care
    // of clearing everything, we do notify if there are probably more parked threads
    // so that spinning won't take place
    atomic_store(&mutex->byte, result.may_have_more_threads ? HAS_PARKED : 0);

    return TOKEN_BARGING_OPPORTUNITY;
}

static void mutex_unlock_slow(mutex_t* mutex) {
    // We could get here because the weak CAS in unlock() failed spuriously, or because there is
    // someone parked. So, we need a CAS loop: even if right now the lock is just held, it could
    // be held and parked if someone attempts to lock just as we are unlocking.
    for (;;) {
        uint8_t old_byte_value = atomic_load(&mutex->byte);
        ASSERT(
            old_byte_value == IS_HELD ||
            old_byte_value == (IS_HELD | HAS_PARKED)
        );

        if (old_byte_value == IS_HELD) {
            uint8_t temp = old_byte_value;
            if (atomic_compare_exchange_weak(&mutex->byte, &temp, old_byte_value & ~IS_HELD))
                return;
            continue;
        }

        // Someone is parked. Unpark exactly one thread. We may hand the lock to that thread
        // directly, or we will unlock the lock at the same time as we unpark to allow for barging.
        // When we unlock, we may leave the parked bit set if there is a chance that there are still
        // other threads parked.
        ASSERT(old_byte_value == (IS_HELD | HAS_PARKED));
        unpark_one(
            mutex,
            (unpark_callback_t)mutex_unpark_callback,
            mutex
        );
        return;
    }
}

static bool mutex_unlock_fast_assuming_zero(mutex_t* mutex) {
    uint8_t is_held = IS_HELD;
    return atomic_compare_exchange_weak_explicit(&mutex->byte, &is_held, 0, memory_order_release, memory_order_release);
}

void mutex_unlock(mutex_t* mutex) {
    if (UNLIKELY(!mutex_unlock_fast_assuming_zero(mutex))) {
        mutex_unlock_slow(mutex);
    }
}
