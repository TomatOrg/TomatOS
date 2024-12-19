#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct park_result {
    size_t unpark_token;
    bool invalid;
    bool timed_out;
} park_result_t;

typedef struct unpark_result {
    // The number of threads that were unparked.
    size_t unparked_threads;

    // The number of threads that were requeued.
    size_t requeued_threads;

    // Whether there are any threads remaining in the queue. This only returns
    // true if a thread was unparked.
    bool have_more_threads;

    // This is set to true on average once every 0.5ms for any given key. It
    // should be used to switch to a fair unlocking mechanism for a particular
    // unlock.
    bool be_fair;
} unpark_result_t;

typedef bool (*parking_lot_park_validate_t)(void* arg);

typedef void (*parking_lot_park_before_sleep_t)(void* arg);

typedef void (*parking_lot_park_timed_out_t)(void* arg, size_t key, bool was_last_thread);

park_result_t parking_lot_park(
    size_t key,
    parking_lot_park_validate_t validate,
    parking_lot_park_before_sleep_t before_sleep,
    parking_lot_park_timed_out_t timed_out,
    void* arg,
    size_t park_token,
    uint64_t deadline
);

typedef size_t (*parking_lot_unpark_callback)(void* arg, unpark_result_t result);

unpark_result_t parking_lot_unpark_one(
    size_t key,
    parking_lot_unpark_callback callback,
    void* arg
);
