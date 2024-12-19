//
// Created by tomato on 12/19/24.
//

#include "parking_lot.h"

#include <mem/alloc.h>
#include <thread/scheduler.h>
#include <thread/thread.h>
#include <time/tsc.h>

#include "word_lock.h"

__attribute__((aligned(64)))
typedef struct parking_lot_bucket {
    // mutex to protect the bucket itself
    word_lock_t mutex;

    // Linked list of threads waiting on this bucket
    thread_t* queue_head;
    thread_t* queue_tail;

    // Next time at which point be_fair should be set
    uint64_t fair_deadline;
    uint32_t fair_seed;

    uint8_t _padding[24];
} parking_lot_bucket_t;
STATIC_ASSERT(sizeof(parking_lot_bucket_t) == 64);

typedef struct parking_lot_hash_table {
    // Previous table. This is only kept to keep leak detectors happy.
    struct parking_lot_hash_table* prev;

    // Number of bits used for the hash function
    uint32_t hash_bits;

    // how many buckets we have
    uint32_t entries_count;

    // the hash buckets for the table
    parking_lot_bucket_t entries[];
} parking_lot_hash_table_t;

/**
 * Even with 3x more buckets than threads, the memory overhead per thread is
 * still only a few hundred bytes per thread.
 */
#define PARKING_LOT_LOAD_FACTOR 3

/**
 * The hash table we are using
 */
static _Atomic(parking_lot_hash_table_t*) m_parking_lot_hash_table = NULL;

/**
 * Keep track of live threads objects and resize the
 * hash table accordingly
 */
static _Atomic(size_t) m_num_threads = 0;


static parking_lot_hash_table_t* parking_lot_new_hash_table(size_t num_threads, parking_lot_hash_table_t* prev) {
    size_t new_size = num_threads * (PARKING_LOT_LOAD_FACTOR * sizeof(parking_lot_hash_table_t));
    new_size = 1 << (64 - __builtin_clzll(new_size - 1));
    uint32_t hash_bits = 64 - __builtin_clzll(new_size) - 1;

    // allocate the table
    parking_lot_hash_table_t* new_hash_table = mem_alloc(sizeof(parking_lot_hash_table_t) + new_size * sizeof(parking_lot_bucket_t));
    ASSERT(new_hash_table != NULL);
    memset(new_hash_table, 0, sizeof(*new_hash_table));

    // set the fields
    new_hash_table->entries_count = new_size;
    new_hash_table->hash_bits = hash_bits;
    new_hash_table->prev = prev;

    // initialize all the entries
    uint64_t now = tsc_get_usecs();
    for (int i = 0; i < PARKING_LOT_LOAD_FACTOR; i++) {
        new_hash_table->entries[i].fair_deadline = now;
        new_hash_table->entries[i].fair_seed = i + 1;
    }

    return new_hash_table;
}

__attribute__((cold))
static parking_lot_hash_table_t* parking_lot_create_hash_table(void) {
    parking_lot_hash_table_t* new_table = parking_lot_new_hash_table(PARKING_LOT_LOAD_FACTOR, NULL);

    parking_lot_hash_table_t* old_table = NULL;
    if (atomic_compare_exchange_strong_explicit(
        &m_parking_lot_hash_table,
        &old_table, new_table,
        memory_order_acq_rel, memory_order_acquire
    )) {
        return new_table;
    }

    // we can free the new table since we
    // already have a table
    mem_free(new_table);

    return old_table;
}

static parking_lot_hash_table_t* parking_lot_get_hash_table(void) {
    parking_lot_hash_table_t* table = atomic_load_explicit(&m_parking_lot_hash_table, memory_order_acquire);
    if (table != NULL) {
        return table;
    }

    return parking_lot_create_hash_table();
}

static size_t parking_lot_hash(size_t key, uint32_t bits) {
    return (key * 0x9E3779B97F4A7C15) >> (64 - bits);
}

static void parking_lot_rehash_bucket_into(parking_lot_bucket_t* bucket, parking_lot_hash_table_t* table) {
    thread_t* current = bucket->queue_head;
    while (current != NULL) {
        // calculate the next thread and the hash code for the new table
        thread_t* next = current->park_next_in_queue;
        uint64_t hash = parking_lot_hash(atomic_load_explicit(&current->park_key, memory_order_relaxed), table->hash_bits);

        // append to the end of the bucket
        if (table->entries[hash].queue_tail == NULL) {
            table->entries[hash].queue_head = current;
        } else {
            table->entries[hash].queue_tail->park_next_in_queue = current;
        }
        table->entries[hash].queue_tail = current;

        // and go to the next entry
        current->park_next_in_queue = NULL;
        current = next;
    }
}

static void parking_lot_grow_hashtable(size_t num_threads) {
    parking_lot_hash_table_t* old_table = NULL;
    for (;;) {
        old_table = parking_lot_get_hash_table();

        // Check if we need to resize the existing table
        if (old_table->entries_count >= PARKING_LOT_LOAD_FACTOR * num_threads) {
            return;
        }

        // Lock all buckets in the old table
        for (int i = 0; i < old_table->entries_count; i++) {
            word_lock_lock(&old_table->entries[i].mutex);
        }

        // Now check if our table is still the latest one. Another thread could
        // have grown the hash table between us reading HASHTABLE and locking
        // the buckets.
        if (atomic_load_explicit(&m_parking_lot_hash_table, memory_order_relaxed) == old_table) {
            break;
        }

        // Unlock buckets and try again
        for (int i = 0; i < old_table->entries_count; i++) {
            word_lock_unlock(&old_table->entries[i].mutex);
        }
    }

    // Create the new table
    parking_lot_hash_table_t* new_table = parking_lot_new_hash_table(num_threads, old_table);

    // Move the entries from the old table to the new one
    for (int i = 0; i < old_table->entries_count; i++) {
        parking_lot_rehash_bucket_into(&old_table->entries[i], new_table);
    }

    // Publish the new table. No races are possible at this point because
    // any other thread trying to grow the hash table is blocked on the bucket
    // locks in the old table.
    atomic_store_explicit(&m_parking_lot_hash_table, new_table, memory_order_release);

    for (int i = 0; i < old_table->entries_count; i++) {
        word_lock_unlock(&old_table->entries[i].mutex);
    }
}

/**
 * Locks the bucket for the given key and returns a reference to it.
 * The returned bucket must be unlocked again in order to not cause deadlocks.
 */
static parking_lot_bucket_t* parking_lot_lock_bucket(size_t key) {
    for (;;) {
        parking_lot_hash_table_t* table = parking_lot_get_hash_table();

        uint64_t hash = parking_lot_hash(key, table->hash_bits);
        parking_lot_bucket_t* bucket = &table->entries[hash];

        // Lock the bucket
        word_lock_lock(&bucket->mutex);

        // If no other thread has rehashed the table before we grabbed the lock
        // then we are good to go! The lock we grabbed prevents any rehashes.
        if (atomic_load_explicit(&m_parking_lot_hash_table, memory_order_relaxed) == table) {
            return bucket;
        }

        // Unlock the bucket and try again
        word_lock_unlock(&bucket->mutex);
    }
}

/**
 * Locks the bucket for the given key and returns a reference to it. But checks that the key
 * hasn't been changed in the meantime due to a requeue.
 * The returned bucket must be unlocked again in order to not cause deadlocks.
 */
static parking_lot_bucket_t* parking_lot_lock_bucket_checked(_Atomic(size_t)* key, size_t* out_key) {
    for (;;) {
        parking_lot_hash_table_t* table = parking_lot_get_hash_table();
        size_t current_key = atomic_load_explicit(key, memory_order_relaxed);

        size_t hash = parking_lot_hash(current_key, table->hash_bits);
        parking_lot_bucket_t* bucket = &table->entries[hash];

        // Lock the bucket
        word_lock_lock(&bucket->mutex);

        // Check that both the hash table and key are correct while the bucket
        // is locked. Note that the key can't change once we locked the proper
        // bucket for it, so we just keep trying until we have the correct key.
        if (
            atomic_load_explicit(&m_parking_lot_hash_table, memory_order_relaxed) == table &&
            atomic_load_explicit(key, memory_order_relaxed) == current_key
        ) {
            *out_key = current_key;
            return bucket;
        }
    }
}

park_result_t parking_lot_park(
    size_t key,
    parking_lot_park_validate_t validate,
    parking_lot_park_before_sleep_t before_sleep,
    parking_lot_park_timed_out_t timed_out,
    void* arg,
    size_t park_token,
    uint64_t deadline
) {
    // get the thread, if it was not seen before increment the load
    thread_t* thread = scheduler_get_current_thread();
    if (!thread->parking_lot_seen) {
        size_t num_threads = atomic_fetch_add_explicit(&m_num_threads, 1, memory_order_relaxed) + 1;
        parking_lot_grow_hashtable(num_threads);
        thread->parking_lot_seen = true;
    }

    // Lock the bucket for the given key
    parking_lot_bucket_t* bucket = parking_lot_lock_bucket(key);

    // If the validation function fails, just return
    if (!validate(arg)) {
        word_lock_unlock(&bucket->mutex);
        return (park_result_t){};
    }

    // Append our thread to the queue and unlock the bucket
    thread->parked_with_timeout = deadline != 0;
    thread->park_next_in_queue = NULL;
    thread->park_key = key;
    thread->park_token = park_token;
    // TODO: prepare park
    if (bucket->queue_head != NULL) {
        bucket->queue_tail->park_next_in_queue = thread;
    } else {
        bucket->queue_head = thread;
    }
    bucket->queue_tail = thread;

    word_lock_unlock(&bucket->mutex);

    // Invoke the pre-sleep callback
    before_sleep(arg);

    // Park our thread and determine whether we were woken up by an unpark
    // or by our timeout. Note that this isn't precise, we can still be unparked
    // since we are still in the queue
    bool unparked = false;
    if (deadline != 0) {
        ASSERT(!"TODO: park with deadline");
    } else {
        scheduler_park();
        unparked = true;
    }

    // If we were unparked, return now
    if (unparked) {
        return (park_result_t){ .unpark_token = thread->unpark_token };
    }

    // Lock our bucket again. Note that the hashtable may have been rehashed in
    // the meantime. Our key may also have changed if we were requeued.
    bucket = parking_lot_lock_bucket_checked(&thread->park_key, &key);

    // Now we need to check again if we were unparked or timed out. Unlike the
    // last check this is precise because we hold the bucket lock.
    // TODO: this
    bool did_time_out = false;
    if (!did_time_out) {
        word_lock_unlock(&bucket->mutex);
        return (park_result_t){ .unpark_token = thread->unpark_token };
    }

    // We timed out, so we now need to remove our thread from the queue
    thread_t** link = &bucket->queue_head;
    thread_t* current = bucket->queue_head;
    thread_t* previous = NULL;
    bool was_last_thread = true;
    while (current != NULL) {
        if (current == thread) {

            // remove the entry from the list
            thread_t* next = current->park_next_in_queue;
            *link = next;

            // update the tail and head if need be
            if (bucket->queue_tail == current) {
                bucket->queue_tail = previous;
            } else {
                // Scan the rest of the queue to see if there are any other
                // entries with the given key.
                thread_t* scan = next;
                while (scan != NULL) {
                    if (atomic_load_explicit(&scan->park_key, memory_order_relaxed) == key) {
                        was_last_thread = false;
                        break;
                    }
                    scan = scan->park_next_in_queue;
                }
            }

            // Callback to indicate that we timed out, and whether we were the
            // last thread on the queue
            timed_out(arg, key, was_last_thread);
            break;
        } else {
            if (atomic_load_explicit(&current->park_key, memory_order_relaxed) == key) {
                was_last_thread = false;
            }

            link = &current->park_next_in_queue;
            previous = current;
            current = *link;
        }
    }

    // There should be no way for our thread to have been removed from the queue
    // if we timed out.
    ASSERT(current != NULL);

    // Unlock the bucket, we are done
    word_lock_unlock(&bucket->mutex);

    return (park_result_t){ .timed_out = true };
}

// Pseudorandom number generator from the "Xorshift RNGs" paper by George Marsaglia.
static uint32_t parking_lot_gen_u32(parking_lot_bucket_t* bucket) {
    bucket->fair_seed ^= bucket->fair_seed << 13;
    bucket->fair_seed ^= bucket->fair_seed >> 17;
    bucket->fair_seed ^= bucket->fair_seed << 5;
    return bucket->fair_seed;
}

static bool parking_lot_should_be_fair(parking_lot_bucket_t* bucket) {
    uint64_t now = tsc_get_usecs();
    if (now > bucket->fair_deadline) {
        // Time between 0 and 1ms
        uint64_t micros = parking_lot_gen_u32(bucket) % 1000;
        bucket->fair_deadline = now + micros;
        return true;
    } else {
        return false;
    }
}

unpark_result_t parking_lot_unpark_one(
    size_t key,
    parking_lot_unpark_callback callback,
    void* arg
) {
    // Lock the bucket for the given key
    parking_lot_bucket_t* bucket = parking_lot_lock_bucket(key);

    // Find a thread with a matching key and remove it from the queue
    thread_t** link = &bucket->queue_head;
    thread_t* current = bucket->queue_head;
    thread_t* previous = NULL;
    unpark_result_t result = {};
    while (current != NULL) {
        if (atomic_load_explicit(&current->park_key, memory_order_relaxed) == key) {

            // Remove the thread form the queue
            thread_t* next = current->park_next_in_queue;
            *link = next;
            if (bucket->queue_tail == current) {
                bucket->queue_tail = previous;
            } else {
                // Scan the rest of the queue to see if there are any other
                // entries with the given key.
                thread_t* scan = next;
                while (scan != NULL) {
                    if (atomic_load_explicit(&scan->park_key, memory_order_relaxed) == key) {
                        result.have_more_threads = true;
                        break;
                    }
                    scan = scan->park_next_in_queue;
                }
            }

            // Invoke the callback before waking up the thread
            result.unparked_threads = 1;
            result.be_fair = parking_lot_should_be_fair(bucket);
            size_t token = callback(arg, result);

            // Set the token for the target thread
            current->unpark_token = token;

            // unpark the current
            scheduler_ready(current);

            // unlock the bucket
            word_lock_unlock(&bucket->mutex);

            return result;

        } else {
            link = &current->park_next_in_queue;
            previous = current;
            current = *link;
        }
    }

    // No threads with matching key were found in the bucket
    callback(arg, result);
    word_lock_unlock(&bucket->mutex);

    return result;
}
