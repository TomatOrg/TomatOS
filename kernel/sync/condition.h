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

#include <stdatomic.h>
#include "mutex.h"

// This is a condition variable that is suitable for use with any lock-like object, including
// our own WTF::Lock. It features standard wait()/notifyOne()/notifyAll() methods in addition to
// a variety of wait-with-timeout methods. This includes methods that use WTF's own notion of
// time, like wall-clock time (i.e. WallTime) and monotonic time (i.e. MonotonicTime). This is
// a very efficient condition variable. It only requires one byte of memory. notifyOne() and
// notifyAll() require just a load and branch for the fast case where no thread is waiting.
// This condition variable, when used with WTF::Lock, can outperform a system condition variable
// and lock by up to 58x.
typedef struct condition {
    atomic_bool has_waiters;
} condition_t;

#define INIT_CONDITION() ((condition_t){ .has_waiters = 0 })

// Wait on a parking queue while releasing the given lock. It will unlock the lock just before
// parking, and relock it upon wakeup. Returns true if we woke up due to some call to
// notifyOne() or notifyAll(). Returns false if we woke up due to a timeout. Note that this form
// of waitUntil() has some quirks:
//
// No spurious wake-up: in order for this to return before the timeout, some notifyOne() or
// notifyAll() call must have happened. No scenario other than timeout or notify can lead to this
// method returning. This means, for example, that you can't use pthread cancelation or signals to
// cause early return.
//
// Past timeout: it's possible for waitUntil() to be called with a timeout in the past. In that
// case, waitUntil() will still release the lock and reacquire it. waitUntil() will always return
// false in that case. This is subtly different from some pthread_cond_timedwait() implementations,
// which may not release the lock for past timeout. But, this behavior is consistent with OpenGroup
// documentation for timedwait().
bool condition_wait(condition_t* condition, mutex_t* mutex, intptr_t timeout);

bool condition_notify_one(condition_t* condition);

void condition_notify_all(condition_t* condition);
