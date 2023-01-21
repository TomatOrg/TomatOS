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

#include "parking_lot.h"
#include "time/tick.h"
#include "util/fastrand.h"
#include "util/stb_ds.h"

#include <thread/scheduler.h>
#include <thread/timer.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// hashtable bucket handling
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct bucket {
    thread_t* queue_head;
    thread_t* queue_tail;

    // This lock protects the entire bucket. Thou shall not make changes to Bucket without holding
    // this lock.
    word_lock_t lock;

    int64_t next_fair_time;

    char padding[64];
} bucket_t;

static void bucket_enqueue(bucket_t* bucket, thread_t* data) {
    ASSERT(data->address != NULL);
    ASSERT(data->next_in_queue == NULL);

    if (bucket->queue_tail != NULL) {
        bucket->queue_head->next_in_queue = data;
        bucket->queue_tail = data;
        return;
    }

    bucket->queue_head = data;
    bucket->queue_tail = data;
}

typedef enum dequeue_result {
    // ignore the element
    IGNORE,

    // remove it and continue to the next element
    REMOVE_AND_CONTINUE,

    // remove it and stop iterating
    REMOVE_AND_STOP,
} dequeue_result_t;

/**
 * Called to figure what we should do with the given element
 */
typedef dequeue_result_t (*park_dequeue_t)(thread_t* element, bool fair, void* ctx);

/**
 * Called after the dequeue is finished but still inside of the lock
 */
typedef void (*park_dequeue_finish_t)(bool result, void* ctx);

/**
 * This is a generic dequeue iterator, on each element it will call
 * the callback and decide if it should remove it or not
 */
static void bucket_dequeue_generic(bucket_t* bucket, park_dequeue_t dequeue, void* ctx) {
    if (bucket->queue_head == NULL) {
        return;
    }

    // This loop is a very clever abomination. The induction variables are the pointer to the
    // pointer to the current node, and the pointer to the previous node. This gives us everything
    // we need to both proceed forward to the next node, and to remove nodes while maintaining the
    // queueHead/queueTail and all of the nextInQueue links. For example, when we are at the head
    // element, then removal means rewiring queueHead, and if it was also equal to queueTail, then
    // we'd want queueTail to be set to nullptr. This works because:
    //
    //     currentPtr == &queueHead
    //     previous == nullptr
    //
    // We remove by setting *currentPtr = (*currentPtr)->nextInQueue, i.e. changing the pointer
    // that used to point to this node to instead point to this node's successor. Another example:
    // if we were at the second node in the queue, then we'd have:
    //
    //     currentPtr == &queueHead->nextInQueue
    //     previous == queueHead
    //
    // If this node is not equal to queueTail, then removing it simply means making
    // queueHead->nextInQueue point to queueHead->nextInQueue->nextInQueue (which the algorithm
    // achieves by mutating *currentPtr). If this node is equal to queueTail, then we want to set
    // queueTail to previous, which in this case is queueHead - thus making the queue look like a
    // proper one-element queue with queueHead == queueTail.
    bool should_continue = true;
    thread_t** current_ptr = &bucket->queue_head;
    thread_t* previous = NULL;

    intptr_t time = get_tick();
    bool time_to_be_fair = false;
    if (time > bucket->next_fair_time)
        time_to_be_fair = true;

    bool did_dequeue = false;

    while (should_continue) {
        thread_t* current = *current_ptr;
        if (current == NULL)
            break;

        dequeue_result_t result = dequeue(current, time_to_be_fair, ctx);
        switch (result) {
            case IGNORE:
                previous = current;
                current_ptr = &(*current_ptr)->next_in_queue;
                break;

            case REMOVE_AND_STOP:
                should_continue = false;
                // fallthrough
            case REMOVE_AND_CONTINUE:
                if (current == bucket->queue_tail)
                    bucket->queue_tail = previous;
                did_dequeue = true;
                *current_ptr = current->next_in_queue;
                current->next_in_queue = NULL;
                break;
        }
    }

    if (time_to_be_fair && did_dequeue)
        bucket->next_fair_time = time + (fastrand() % TICKS_PER_MILLISECOND);

    ASSERT(!!bucket->queue_head == !!bucket->queue_tail);
}

static dequeue_result_t dequeue_first(thread_t* element, bool fair, thread_t** result) {
    *result = element;
    return REMOVE_AND_STOP;
}

static thread_t* bucket_dequeue(bucket_t* bucket) {
    thread_t* result = NULL;
    bucket_dequeue_generic(
        bucket,
        (void*)dequeue_first,
        &result
    );
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Hashtable for the parking lot
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// With 64 bytes of padding per bucket, assuming a hashtable is fully populated with buckets, the
// memory usage per thread will still be less than 1KB.
#define MAX_LOAD_FACTOR 3

#define GROWTH_FACTOR 2

// Thomas Wang's 64 bit Mix Function: http://www.cris.com/~Ttwang/tech/inthash.htm
static unsigned hash_address(const void* address) {
    uint64_t key = (uint64_t)address;
    key += ~(key << 32);
    key ^= (key >> 22);
    key += ~(key << 13);
    key ^= (key >> 8);
    key += (key << 3);
    key ^= (key >> 15);
    key += ~(key << 27);
    key ^= (key >> 31);
    return key;
}

typedef struct hashtable {
    unsigned size;
    _Atomic(bucket_t*) data[0];
} hashtable_t;

/**
 * A lock to protect the allocated hashtables
 */
static word_lock_t m_hashtables_lock = { 0 };

/**
 * The hashtables that we have allocated
 */
static hashtable_t** m_hashtables = NULL;

/**
 * Create a new hashtable of the given size
 */
static hashtable_t* hashtable_create(unsigned size) {
    ASSERT(size >= 1);

    size_t allocated_size = sizeof(hashtable_t) + sizeof(_Atomic(bucket_t*)) * size;
    hashtable_t* result = malloc(allocated_size);
    if (result == NULL) {
        PANIC_ON(ERROR_OUT_OF_MEMORY);
    }
    memset(result, 0, allocated_size);
    result->size = size;

    // This is not fast and it's not data-access parallel, but that's fine, because
    // hashtable resizing is guaranteed to be rare and it will never happen in steady
    // state.
    word_lock_lock(&m_hashtables_lock);
    arrpush(m_hashtables, result);
    word_lock_unlock(&m_hashtables_lock);

    return result;
}

static void destroy_hashtable(hashtable_t* hashtable) {
    word_lock_lock(&m_hashtables_lock);
    for (int i = 0; i < arrlen(m_hashtables); i++) {
        if (m_hashtables[i] == hashtable) {
            arrdelswap(m_hashtables, i);
            break;
        }
    }
    word_lock_unlock(&m_hashtables_lock);

    free(hashtable);
}

/**
 * The active hashtable we have
 */
static _Atomic(hashtable_t*) m_hashtable = NULL;

static hashtable_t* ensure_hashtable() {
    for (;;) {
        hashtable_t* current_hashtable = atomic_load(&m_hashtable);

        if (current_hashtable != NULL)
            return current_hashtable;

        if (current_hashtable == NULL) {
            current_hashtable = hashtable_create(MAX_LOAD_FACTOR);

            hashtable_t* null = NULL;
            if (atomic_compare_exchange_weak(&m_hashtable, &null, current_hashtable)) {
                return current_hashtable;
            }

            destroy_hashtable(current_hashtable);
        }
    }
}

// Locks the hashtable. This reloops in case of rehashing, so the current hashtable may be different
// after this returns than when you called it. Guarantees that there is a hashtable. This is pretty
// slow and not scalable, so it's only used during thread creation and for debugging/testing.
static bucket_t** lock_hashtable() {
    for (;;) {
        hashtable_t* current_hashtable = ensure_hashtable();
        ASSERT(current_hashtable != NULL);

        // Now find all of the buckets. This makes sure that the hashtable is full of buckets so that
        // we can lock all of the buckets, not just the ones that are materialized.
        bucket_t** buckets = NULL;
        for (unsigned i = current_hashtable->size; i--; ) {
            _Atomic(bucket_t*)* bucket_pointer = &current_hashtable->data[i];

            for (;;) {
                bucket_t* bucket = atomic_load(bucket_pointer);
                if (bucket == NULL) {
                    bucket = malloc(sizeof(bucket_t));
                    if (bucket == NULL) {
                        PANIC_ON(ERROR_OUT_OF_MEMORY);
                    }
                    bucket->queue_tail = NULL;
                    bucket->queue_head = NULL;

                    bucket_t* null = NULL;
                    if (!atomic_compare_exchange_weak(bucket_pointer, &null, bucket)) {
                        free(bucket);
                        continue;
                    }
                }

                arrpush(buckets, bucket);
                break;
            }
        }

        // Now lock the buckets in the right order.
        // TODO: sort
        for (int i = 0; i < arrlen(buckets); i++) {
            word_lock_lock(&buckets[i]->lock);
        }


        // If the hashtable didn't change (wasn't rehashed) while we were locking it, then we own it
        // now.
        if (atomic_load(&m_hashtable) == current_hashtable)
            return buckets;

        // The hashtable rehashed. Unlock everything and try again.
        for (int i = 0; i < arrlen(buckets); i++) {
            word_lock_unlock(&buckets[i]->lock);
        }
        arrfree(buckets);
    }
}

static void unlock_hashtable(bucket_t** buckets) {
    for (int i = 0; i < arrlen(buckets); i++) {
        word_lock_unlock(&buckets[i]->lock);
    }
    arrfree(buckets);
}

static void ensure_hashtable_size(unsigned num_threads) {
    // We try to ensure that the size of the hashtable used for thread queues is always large enough
    // to avoid collisions. So, since we started a new thread, we may need to increase the size of the
    // hashtable. This does just that. Note that we never free the old spine, since we never lock
    // around spine accesses (i.e. the "hashtable" global variable).

    // First do a fast check to see if rehashing is needed.
    hashtable_t* old_hashtable = atomic_load(&m_hashtable);
    if (old_hashtable != NULL && old_hashtable->size / num_threads >= MAX_LOAD_FACTOR) {
        return;
    }

    // Seems like we *might* have to rehash, so lock the hashtable and try again.
    bucket_t** buckets_to_unlock = lock_hashtable();

    // Check again, since the hashtable could have rehashed while we were locking it. Also,
    // lockHashtable() creates an initial hashtable for us.
    old_hashtable = atomic_load(&m_hashtable);
    ASSERT(old_hashtable != NULL);
    if (old_hashtable->size / num_threads >= MAX_LOAD_FACTOR) {
        unlock_hashtable(buckets_to_unlock);
        return;
    }

    bucket_t** reusable_buckets = NULL;
    arrsetlen(reusable_buckets, arrlen(buckets_to_unlock));
    memcpy(reusable_buckets, buckets_to_unlock, arrlen(buckets_to_unlock) * sizeof(buckets_to_unlock));

    // OK, now we resize. First we gather all thread datas from the old hashtable. These thread datas
    // are placed into the vector in queue order.
    thread_t** threads = NULL;
    for (int i = 0; i < arrlen(reusable_buckets); i++) {
        bucket_t* bucket = reusable_buckets[i];
        thread_t* thread;
        while ((thread = bucket_dequeue(bucket)) != NULL) {
            arrpush(threads, thread);
        }
    }

    unsigned new_size = num_threads * GROWTH_FACTOR * MAX_LOAD_FACTOR;
    ASSERT(new_size > old_hashtable->size);

    hashtable_t* new_hashtable = hashtable_create(new_size);
    for (int i = 0; i < arrlen(threads); i++) {
        thread_t* thread = threads[i];
        unsigned hash = hash_address(thread->address);
        unsigned index = hash % new_hashtable->size;
        bucket_t* bucket = atomic_load(&new_hashtable->data[index]);
        if (bucket == NULL) {
            if (arrlen(reusable_buckets) == 0) {
                bucket = malloc(sizeof(bucket_t));
                if (bucket == NULL) {
                    PANIC_ON(ERROR_OUT_OF_MEMORY);
                }
                bucket->queue_head = NULL;
                bucket->queue_tail = NULL;
            } else {
                bucket = arrpop(reusable_buckets);
            }
            atomic_store(&new_hashtable->data[index], bucket);
        }
        bucket_enqueue(bucket, thread);
    }
    arrfree(threads);

    // At this point there may be some buckets left unreused. This could easily happen if the
    // number of enqueued threads right now is low but the high watermark of the number of threads
    // enqueued was high. We place these buckets into the hashtable basically at random, just to
    // make sure we don't leak them.
    for (unsigned i = 0; i < new_hashtable->size && arrlen(reusable_buckets) != 0; ++i) {
        _Atomic(bucket_t*)* bucket_ptr = &new_hashtable->data[i];
        if (atomic_load(bucket_ptr) != NULL) {
            continue;
        }
        atomic_store(bucket_ptr, arrpop(reusable_buckets));
    }

    // Since we increased the size of the hashtable, we should have exhausted our preallocated
    // buckets by now.
    ASSERT(arrlen(reusable_buckets) == 0);
    arrfree(reusable_buckets);

    // OK, right now the old hashtable is locked up and the new hashtable is ready to rock and
    // roll. After we install the new hashtable, we can release all bucket locks.

    bool result = atomic_compare_exchange_strong(&m_hashtable, &old_hashtable, new_hashtable);
    ASSERT(result);

    unlock_hashtable(buckets_to_unlock);
}

void parking_lot_rehash(int thread_count) {
    ensure_hashtable_size(thread_count);
}


typedef enum bucket_mode {
    ENSURE_NON_EMPTY,
    IGNORE_EMPTY
} bucket_mode_t;

/**
 * Perform a dequeue operation with the sync block
 * being locked, then when done perform a finish
 * operation with the lock still locked
 *
 * Returns true if the queue might still have elements
 * in it, and false if it is empty.
 */
static bool park_dequeue(
    const void* address,
    bucket_mode_t bucket_mode,
    park_dequeue_t dequeue,
    park_dequeue_finish_t finish,
    void* ctx
) {
    unsigned hash = hash_address(address);

    for (;;) {
        hashtable_t* my_hashtable = ensure_hashtable();
        unsigned index = hash % my_hashtable->size;
        _Atomic(bucket_t*)* bucket_pointer = &my_hashtable->data[index];
        bucket_t* bucket = atomic_load(bucket_pointer);

        if (bucket == NULL) {
            if (bucket_mode == IGNORE_EMPTY) {
                return false;
            }

            for (;;) {
                bucket = atomic_load(bucket_pointer);
                if (bucket == NULL) {
                    bucket = malloc(sizeof(bucket_t));
                    if (bucket == NULL) {
                        PANIC_ON(ERROR_OUT_OF_MEMORY);
                    }
                    bucket->queue_tail = NULL;
                    bucket->queue_head = NULL;

                    bucket_t* null = NULL;
                    if (!atomic_compare_exchange_weak(bucket_pointer, &null, bucket)) {
                        free(bucket);
                        continue;
                    }
                }

                break;
            }
        }

        word_lock_lock(&bucket->lock);

        // At this point the hashtable could have rehashed under us.
        if (atomic_load(&m_hashtable) != my_hashtable) {
            word_lock_unlock(&bucket->lock);
            continue;
        }

        bucket_dequeue_generic(bucket, dequeue, ctx);
        bool result = bucket->queue_head != NULL;
        if (finish != NULL) finish(result, ctx);

        word_lock_unlock(&bucket->lock);
        return result;
    }
}

/**
 * Perform a validation with the sync block being locked and
 * then if the validation passes enqueue the thread to the
 * sync block
 */
static bool park_enqueue(const void* address, thread_t* me, park_validation_t validation, void* ctx) {
    unsigned hash = hash_address(address);

    for (;;) {
        hashtable_t* my_hashtable = ensure_hashtable();
        unsigned index = hash % my_hashtable->size;
        _Atomic(bucket_t*)* bucket_pointer = &my_hashtable->data[index];
        bucket_t* bucket;
        for (;;) {
            bucket = atomic_load(bucket_pointer);
            if (bucket == NULL) {
                bucket = malloc(sizeof(bucket_t));
                if (bucket == NULL) {
                    PANIC_ON(ERROR_OUT_OF_MEMORY);
                }
                bucket->queue_tail = NULL;
                bucket->queue_head = NULL;

                bucket_t* null = NULL;
                if (!atomic_compare_exchange_weak(bucket_pointer, &null, bucket)) {
                    free(bucket);
                    continue;
                }
            }
            break;
        }

        word_lock_lock(&bucket->lock);

        if (atomic_load(&m_hashtable) != my_hashtable) {
            word_lock_unlock(&bucket->lock);
            continue;
        }

        bool queued = false;
        if (validation(ctx)) {
            me->address = address;
            bucket_enqueue(bucket, me);
            queued = true;
        }

        word_lock_unlock(&bucket->lock);

        return queued;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual parking api implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * The timer callback, wakes up the given thread from a timeout
 */
static void wakeup_thread(thread_t* thread, uintptr_t now) {
    spinlock_lock(&thread->parking_lock);
    scheduler_ready_thread(thread);
    spinlock_unlock(&thread->parking_lock);
}

typedef struct arg {
    thread_t* target;
    bool did_dequeue;
} dequeue_specific_arg_t;

static dequeue_result_t dequeue_specific_thread(thread_t* element, bool fair, dequeue_specific_arg_t* arg) {
    if (element == arg->target) {
        arg->did_dequeue = true;
        return REMOVE_AND_STOP;
    }
    return IGNORE;
}

park_result_t park_conditionally(
    const void* address,
    park_validation_t validation,
    park_before_sleep_t before_sleep,
    void* ctx,
    intptr_t timeout
) {
    thread_t* me = get_current_thread();
    me->token = 0;

    // Guard against someone calling parkConditionally() recursively from beforeSleep().
    ASSERT(me->address == NULL);

    bool enqueue_result = park_enqueue(address, me, validation, ctx);

    if (!enqueue_result)
        return (park_result_t){};

    if (before_sleep != NULL)
        before_sleep(ctx);

    // if needed setup a timer for when to wake up
    timer_t* timer = NULL;
    intptr_t when = INTPTR_MAX;
    if (timeout >= 0) {
        when = get_tick() + timeout;
        timer = create_timer();
        timer->when = when;
        timer->func = (timer_func_t) wakeup_thread;
        timer->arg = me;
        timer_start(timer);
    }

    // now do the locking loop
    spinlock_lock(&me->parking_lock);
    while (me->address != NULL && get_tick() < when) {
        scheduler_park((void*)spinlock_unlock, &me->parking_lock);
        spinlock_lock(&me->parking_lock);
    }
    ASSERT(me->address == NULL || me->address == address);
    bool did_get_dequeued = me->address == NULL;
    spinlock_unlock(&me->parking_lock);

    // release the timer
    if (timer != NULL) {
        timer_stop(timer);
        SAFE_RELEASE_TIMER(timer);
    }

    if (did_get_dequeued) {
        // Great! We actually got dequeued rather than the timeout expiring.
        return (park_result_t){
            .was_unparked = true,
            .token = me->token
        };
    }

    // Have to remove ourselves from the queue since we timed out and nobody has dequeued us yet.

    dequeue_specific_arg_t arg = {
        .did_dequeue = false,
        .target = me
    };
    park_dequeue(
        address, IGNORE_EMPTY,
        (void*)dequeue_specific_thread,
        NULL,
        &arg
    );
    bool did_dequeue = arg.did_dequeue;

    // If didDequeue is true, then we dequeued ourselves. This means that we were not unparked.
    // If didDequeue is false, then someone unparked us.

    ASSERT(me->next_in_queue == NULL);

    // Make sure that no matter what, me->address is null after this point.
    spinlock_lock(&me->parking_lock);
    if (!did_dequeue) {
        // If we did not dequeue ourselves, then someone else did. They will set our address to
        // null. We don't want to proceed until they do this, because otherwise, they may set
        // our address to null in some distant future when we're already trying to wait for
        // other things.
        while (me->address != NULL) {
            scheduler_park((void*)spinlock_unlock, &me->parking_lock);
            spinlock_lock(&me->parking_lock);
        }
    }
    me->address = NULL;
    spinlock_unlock(&me->parking_lock);

    park_result_t result = {
        .was_unparked = !did_dequeue,
    };
    if (!did_dequeue) {
        // If we were unparked then there should be a token.
        result.token = me->token;
    }
    return result;
}

typedef struct unpark_amount_arg {
    const void* address;
    thread_t** arr;
} unpark_amount_arg_t;

/**
 * Unpark the given amount of threads
 */
static dequeue_result_t unpark_all_callback(thread_t* element, bool time_to_be_fair, unpark_amount_arg_t* arg) {
    if (element->address != arg->address)
        return IGNORE;

    arrpush(arg->arr, put_thread(element));
    return REMOVE_AND_CONTINUE;
}

void unpark_all(const void* address) {
    unpark_amount_arg_t arg = {
        .address = address,
        .arr = NULL,
    };
    park_dequeue(
        address,
        IGNORE_EMPTY,
        (void *) unpark_all_callback,
        NULL,
        &arg
    );

    for (int i = 0; i < arrlen(arg.arr); i++) {
        thread_t* thread = arg.arr[i];
        ASSERT(thread->address != NULL);

        spinlock_lock(&thread->parking_lock);
        thread->address = NULL;
        scheduler_ready_thread(thread);
        spinlock_unlock(&thread->parking_lock);

        release_thread(thread);
    }

    arrfree(arg.arr);
}

typedef struct unpark_first_arg {
    const void* address;
    thread_t* thread;
    bool time_to_be_fair;

    unpark_callback_t callback;
    void* ctx;
} unpark_first_arg_t;

static dequeue_result_t unpark_first(thread_t* element, bool passed_time_to_be_fair, unpark_first_arg_t* arg) {
    if (element->address != arg->address)
        return IGNORE;

    arg->thread = put_thread(element);
    arg->time_to_be_fair = passed_time_to_be_fair;
    return REMOVE_AND_STOP;
}

static void call_unpark_callback(bool may_have_more_threads, unpark_first_arg_t* arg) {
    thread_t* thread = arg->thread;

    unpark_result_t result = { 0 };
    result.did_unpark_thread = thread != NULL;
    result.may_have_more_threads = result.did_unpark_thread && may_have_more_threads;
    if (arg->time_to_be_fair)
        ASSERT(thread != NULL);

    result.time_to_be_fair = arg->time_to_be_fair;
    intptr_t token = arg->callback(result, arg->ctx);
    if (thread != NULL)
        thread->token = token;
}

void unpark_one(
    const void* address,
    unpark_callback_t callback,
    void* ctx
) {
    unpark_first_arg_t arg = {
        .ctx = ctx,
        .address = address,
        .callback = callback,
        .time_to_be_fair = false,
    };
    park_dequeue(
        address,
        ENSURE_NON_EMPTY,
        (void*)unpark_first,
        (void*)call_unpark_callback,
        &arg
    );
    thread_t* thread = arg.thread;

    if (thread == NULL)
        return;

    ASSERT(thread->address != NULL);

    spinlock_lock(&thread->parking_lock);
    thread->address = NULL;
    scheduler_ready_thread(thread);
    spinlock_unlock(&thread->parking_lock);

    // At this point, the threadData may die
    release_thread(thread);
}
