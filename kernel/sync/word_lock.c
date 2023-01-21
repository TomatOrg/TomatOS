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

#include "word_lock.h"

#include "spinlock.h"

#include <thread/scheduler.h>

#include <stdatomic.h>

#define IS_LOCKED           1
#define IS_QUEUE_LOCKED     2
#define QUEUE_HEAD_MASK     3

// This magic number turns out to be optimal based on past JikesRVM experiments.
#define SPIN_LIMIT 40

// This data structure serves three purposes:
//
// 1) A parking mechanism for threads that go to sleep. That involves just a system mutex and
//    condition variable.
//
// 2) A queue node for when a thread is on some WordLock's queue.
//
// 3) The queue head. This is kind of funky. When a thread is the head of a queue, it also serves as
//    the basic queue bookkeeping data structure. When a thread is dequeued, the next thread in the
//    queue takes on the queue head duties.
typedef struct thread_data {
    // The parking mechanism
    bool should_park;

    // to protect when sleeping
    spinlock_t parking_lock;

    // the sleeping thread
    thread_t* thread;

    // The queue node
    struct thread_data* next_in_queue;

    // The queue itself
    struct thread_data* queue_tail;
} thread_data_t;

static void word_lock_lock_slow(word_lock_t* lock) {
    thread_t* thread = get_current_thread();
    unsigned spin_count = 0;

    while (true) {
        uintptr_t current_word_value = atomic_load(&lock->lock);

        if (!(current_word_value & IS_LOCKED)) {
            // It's not possible for someone to hold the queue lock while the lock itself is no longer
            // held, since we will only attempt to acquire the queue lock when the lock is held and
            // the queue lock prevents unlock.
            ASSERT(!(current_word_value & IS_QUEUE_LOCKED));

            uintptr_t temp = current_word_value;
            if (atomic_compare_exchange_weak(&lock->lock, &temp, current_word_value | IS_LOCKED)) {
                // Success! We acquired the lock.
                return;
            }
        }

        // If there is no queue and we haven't spun too much, we can just try to spin around again.
        if (!(current_word_value & ~QUEUE_HEAD_MASK) && spin_count < SPIN_LIMIT) {
            spin_count++;
            scheduler_yield();
            continue;
        }

        // Need to put ourselves on the queue. Create the queue if one does not exist. This requries
        // owning the queue for a little bit. The lock that controls the queue is itself a spinlock.

        thread_data_t me = {
            .thread = thread
        };

        // Reload the current word value, since some time may have passed.
        current_word_value = atomic_load(&lock->lock);

        // We proceed only if the queue lock is not held, the WordLock is held, and we succeed in
        // acquiring the queue lock.
        uintptr_t temp = current_word_value;
        if (
            (current_word_value & IS_QUEUE_LOCKED) ||
            !(current_word_value & IS_LOCKED) ||
            !atomic_compare_exchange_weak(&lock->lock, &temp, current_word_value | IS_QUEUE_LOCKED)
        ) {
            scheduler_yield();
            continue;
        }

        me.should_park = true;

        // We own the queue. Nobody can enqueue or dequeue until we're done. Also, it's not possible
        // to release the WordLock while we hold the queue lock.
        thread_data_t* queue_head = (thread_data_t*)(current_word_value & ~QUEUE_HEAD_MASK);
        if (queue_head != NULL) {
            // Put this thread at the end of the queue.
            queue_head->queue_tail->next_in_queue = &me;
            queue_head->queue_tail = &me;

            // Release the queue lock.
            current_word_value = atomic_load(&lock->lock);
            ASSERT(current_word_value & ~QUEUE_HEAD_MASK);
            ASSERT(current_word_value & IS_QUEUE_LOCKED);
            ASSERT(current_word_value & IS_LOCKED);
            atomic_store(&lock->lock, current_word_value & ~IS_QUEUE_LOCKED);
        } else {
            // Make this thread be the queue-head
            queue_head = &me;

            // Release the queue lock and install ourselves as the head. No need for a CAS loop, since
            // we own the queue lock.
            current_word_value = atomic_load(&lock->lock);
            ASSERT(~(current_word_value & ~QUEUE_HEAD_MASK));
            ASSERT(current_word_value & IS_QUEUE_LOCKED);
            ASSERT(current_word_value & IS_LOCKED);
            uintptr_t new_word_value = current_word_value;
            new_word_value |= (uintptr_t)queue_head;
            new_word_value &= ~IS_QUEUE_LOCKED;
            atomic_store(&lock->lock, new_word_value);
        }

        // At this point everyone who acquires the queue lock will see me on the queue, and anyone who
        // acquires me's lock will see that me wants to park. Note that shouldPark may have been
        // cleared as soon as the queue lock was released above, but it will happen while the
        // releasing thread holds me's parkingLock.
        spinlock_lock(&me.parking_lock);
        while (me.should_park) {
            scheduler_park((void*)spinlock_unlock, &me.parking_lock);
            spinlock_lock(&me.parking_lock);
        }
        spinlock_unlock(&me.parking_lock);

        ASSERT(!me.should_park);
        ASSERT(!me.next_in_queue);
        ASSERT(!me.queue_tail);

        // Now we can loop around and try to acquire the lock again.
    }
}

void word_lock_lock(word_lock_t* mutex) {
    uintptr_t zero = 0;
    if (LIKELY(atomic_compare_exchange_weak_explicit(&mutex->lock, &zero, IS_LOCKED, memory_order_acquire, memory_order_acquire))) {
        return;
    }

    word_lock_lock_slow(mutex);
}

static void word_lock_unlock_slow(word_lock_t* mutex) {
    // The fast path can fail either because of spurious weak CAS failure, or because someone put a
    // thread on the queue, or the queue lock is held. If the queue lock is held, it can only be
    // because someone *will* enqueue a thread onto the queue.

    // Acquire the queue lock, or release the lock. This loop handles both lock release in case the
    // fast path's weak CAS spuriously failed and it handles queue lock acquisition if there is
    // actually something interesting on the queue.
    while (true) {
        uintptr_t current_word_value = atomic_load(&mutex->lock);

        ASSERT(current_word_value & IS_LOCKED);

        if (current_word_value == IS_LOCKED) {
            uintptr_t is_locked = IS_LOCKED;
            if (atomic_compare_exchange_weak(&mutex->lock, &is_locked, 0)) {
                // The fast path's weak CAS had spuriously failed, and now we succeeded. The lock is
                // unlocked and we're done!
                return;
            }
            // Loop around and try again.
            scheduler_yield();
            continue;
        }

        if (current_word_value & IS_QUEUE_LOCKED) {
            scheduler_yield();
            continue;
        }

        // If it wasn't just a spurious weak CAS failure and if the queue lock is not held, then there
        // must be an entry on the queue.
        ASSERT(current_word_value & ~QUEUE_HEAD_MASK);

        uintptr_t temp = current_word_value;
        if (atomic_compare_exchange_weak(&mutex->lock, &temp, current_word_value | IS_QUEUE_LOCKED))
            break;
    }

    uintptr_t current_word_value = atomic_load(&mutex->lock);

    // After we acquire the queue lock, the WordLock must still be held and the queue must be
    // non-empty. The queue must be non-empty since only the lockSlow() method could have held the
    // queue lock and if it did then it only releases it after putting something on the queue.
    ASSERT(current_word_value & IS_LOCKED);
    ASSERT(current_word_value & IS_QUEUE_LOCKED);
    thread_data_t* queue_head = (thread_data_t*)(current_word_value & ~QUEUE_HEAD_MASK);
    ASSERT(queue_head != NULL);

    thread_data_t* new_queue_head = queue_head->next_in_queue;
    // Either this was the only thread on the queue, in which case we delete the queue, or there
    // are still more threads on the queue, in which case we create a new queue head.
    if (new_queue_head != NULL) {
        new_queue_head->queue_tail = queue_head->queue_tail;
    }

    // Change the queue head, possibly removing it if new_queue_head is NULL. No need for a CAS loop,
    // since we hold the queue lock and the lock itself so nothing about the lock can change right
    // now.
    current_word_value = atomic_load(&mutex->lock);
    ASSERT(current_word_value & IS_LOCKED);
    ASSERT(current_word_value & IS_QUEUE_LOCKED);
    ASSERT((current_word_value & ~QUEUE_HEAD_MASK) == (uintptr_t)queue_head);
    uintptr_t new_word_value = current_word_value;
    new_word_value &= ~IS_LOCKED; // Release the WordLock.
    new_word_value &= ~IS_QUEUE_LOCKED; // Release the queue lock.
    new_word_value &= QUEUE_HEAD_MASK; // Clear out the old queue head.
    new_word_value |= (uintptr_t)new_queue_head; // Install new queue head.
    atomic_store(&mutex->lock, new_word_value);


    // Now the lock is available for acquisition. But we just have to wake up the old queue head.
    // After that, we're done!

    queue_head->next_in_queue = NULL;
    queue_head->queue_tail = NULL;

    // We do this carefully because this may run either before or during the parking_lock critical
    // section in mutex_lock_slow().
    spinlock_lock(&queue_head->parking_lock);
    queue_head->should_park = false;
    scheduler_ready_thread(queue_head->thread);
    spinlock_unlock(&queue_head->parking_lock);

    // The old queue head can now contend for the lock again. We're done!
}

void word_lock_unlock(word_lock_t* mutex) {
    uintptr_t is_locked = IS_LOCKED;
    if (LIKELY(atomic_compare_exchange_weak_explicit(&mutex->lock, &is_locked, 0, memory_order_release, memory_order_release))) {
        return;
    }

    word_lock_unlock_slow(mutex);
}
