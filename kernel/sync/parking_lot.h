#pragma once

/*
 * CODE TAKEN FROM WebKit WTF library
 *
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

#include "thread/thread.h"
#include "word_lock.h"

typedef struct park_result {
    bool was_unparked;
    intptr_t token;
} park_result_t;

typedef bool (*park_validation_t)(void* ctx);
typedef void (*park_before_sleep_t)(void* ctx);

// Unparking status given to you anytime you unparkOne().
typedef struct unpark_result {
    // True if some thread was unparked.
    bool did_unpark_thread;

    // True if there may be more threads on this address. This may be conservatively true.
    bool may_have_more_threads;

    // This bit is randomly set to true indicating that it may be profitable to unlock the lock
    // using a fair unlocking protocol. This is most useful when used in conjunction with
    // unparkOne(address, callback).
    bool time_to_be_fair;
} unpark_result_t;

typedef intptr_t (*unpark_callback_t)(unpark_result_t result, void* ctx);

// Parks the thread in a queue associated with the given address, which cannot be null. The
// parking only succeeds if the validation function returns true while the queue lock is held.
//
// If validation returns false, it will unlock the internal parking queue and then it will
// return a null ParkResult (wasUnparked = false, token = 0) without doing anything else.
//
// If validation returns true, it will enqueue the thread, unlock the parking queue lock, call
// the beforeSleep function, and then it will sleep so long as the thread continues to be on the
// queue and the timeout hasn't fired. Finally, this returns wasUnparked = true if we actually
// got unparked or wasUnparked = false if the timeout was hit. When wasUnparked = true, the
// token will contain whatever token was returned from the callback to unparkOne(), or 0 if the
// thread was unparked using unparkAll() or the form of unparkOne() that doesn't take a
// callback.
//
// Note that beforeSleep is called with no locks held, so it's OK to do pretty much anything so
// long as you don't recursively call parkConditionally(). You can call unparkOne()/unparkAll()
// though. It's useful to use beforeSleep() to unlock some mutex in the implementation of
// Condition::wait().
park_result_t park_conditionally(
    const void* address,
    park_validation_t validation,
    park_before_sleep_t before_sleep,
    void* ctx,
    intptr_t timeout
);

// Unparks every thread from the queue associated with the given address, which cannot be null.
void unpark_all(const void* address);

// This is an expert-mode version of unparkOne() that allows for really good thundering herd
// avoidance and eventual stochastic fairness in adaptive mutexes.
//
// Unparks one thread from the queue associated with the given address, and calls the given
// callback while the address is locked. Reports to the callback whether any thread got
// unparked, whether there may be any other threads still on the queue, and whether this may be
// a good time to do fair unlocking. The callback returns an intptr_t token, which is returned
// to the unparked thread via ParkResult::token.
//
// WTF::Lock and WTF::Condition both use this form of unparkOne() because it allows them to use
// the ParkingLot's internal queue lock to serialize some decision-making. For example, if
// UnparkResult::mayHaveMoreThreads is false inside the callback, then we know that at that
// moment nobody can add any threads to the queue because the queue lock is still held. Also,
// WTF::Lock uses the timeToBeFair and token mechanism to implement eventual fairness.
void unpark_one(
    const void* address,
    unpark_callback_t callback,
    void* ctx
);

/**
 * Tells parking lot about the current amount of threads we have, which
 * might cause parking lot to rehash the hashtable to accommodate for
 * having new threads
 */
void parking_lot_rehash(int thread_count);
