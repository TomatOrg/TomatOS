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

#include "condition.h"

#include "parking_lot.h"
#include "irq/irq.h"

typedef struct condition_arg {
    condition_t* condition;
    mutex_t* mutex;
} condition_arg_t;

static bool condition_validation(condition_arg_t* arg) {
    atomic_store(&arg->condition->has_waiters, true);
    return true;
}

static void condition_before_sleep(condition_arg_t* arg) {
    mutex_unlock(arg->mutex);
}

bool condition_wait(condition_t* condition, mutex_t* mutex, intptr_t timeout) {
    condition_arg_t arg = {
        .condition = condition,
        .mutex = mutex
    };
    bool result = park_conditionally(
        condition,
        (void*)condition_validation,
        (void*)condition_before_sleep,
        &arg,
        timeout
    ).was_unparked;
    mutex_lock(mutex);
    return result;
}

typedef struct condition_wake_one_arg {
    bool did_notify_thread;
    condition_t* condition;
} condition_wake_one_arg_t;

static intptr_t condition_wake_one(unpark_result_t result, condition_wake_one_arg_t* arg) {
    if (!result.may_have_more_threads) {
        atomic_store(&arg->condition->has_waiters, false);
    }
    arg->did_notify_thread = result.did_unpark_thread;
    return 0;
}

bool condition_notify_one(condition_t* condition) {
    if (!atomic_load(&condition->has_waiters)) {
        // At this exact instant, there is nobody waiting on this condition. The way to visualize
        // this is that if unparkOne() ran to completion without obstructions at this moment, it
        // wouldn't wake anyone up. Hence, we have nothing to do!
        return false;
    }

    condition_wake_one_arg_t arg = {
        .condition = condition
    };
    unpark_one(
        condition,
        (void*)condition_wake_one,
        &arg
    );
    return arg.did_notify_thread;
}

void condition_notify_all(condition_t* condition) {
    if (!atomic_load(&condition->has_waiters)) {
        // See above.
        return;
    }

    // It's totally safe for us to set this to false without any locking, because this thread is
    // guaranteed to then unparkAll() anyway. So, if there is a race with some thread calling
    // wait() just before this store happens, that thread is guaranteed to be awoken by the call to
    // unparkAll(), below.
    atomic_store(&condition->has_waiters, false);

    unpark_all(condition);
}
