#pragma once

#include <stdint.h>
#include "thread.h"

typedef struct wait_queue {
    waiting_thread_t* first;
    waiting_thread_t* last;
} wait_queue_t;

typedef struct waitable {
    size_t count;
    size_t size;
    uint32_t closed;
    wait_queue_t wait_queue;
    wait_queue_t send_queue;
    spinlock_t lock;

    atomic_size_t ref_count;
} waitable_t;

typedef enum waitable_result {
    WAITABLE_EMPTY = 0,
    WAITABLE_CLOSED = 1,
    WAITABLE_SUCCESS = 2,
} waitable_result_t;

/**
 * Create a new waitable of the given size
 */
waitable_t* create_waitable(size_t size);

/**
 * Increase the ref count
 */
waitable_t* put_waitable(waitable_t* waitable);

/**
 * Decrease the ref count, and free if needed
 */
void release_waitable(waitable_t* waitable);

#define SAFE_RELEASE_WAITABLE(waitable) \
    do { \
        if (waitable != NULL) { \
            release_waitable(waitable); \
            waitable = NULL; \
        } \
    } while (0)

/**
 * Send/Write/Signal the w
 */
bool waitable_send(waitable_t* w, bool block);

/**
 * Wait/Recv the waitable
 */
waitable_result_t waitable_wait(waitable_t* waitable, bool block);

/**
 * Close the waitable
 */
void waitable_close(waitable_t* waitable);

typedef struct selected_waitable {
    int index;
    bool success;
} selected_waitable_t;

/**
 * Waits on all the given waitables and waits for one of them
 * to be ready
 */
selected_waitable_t waitable_select(waitable_t** waitables, int send_count, int wait_count, bool block);

/**
 * TODO: better name?
 * Create a waitable that will get triggered after the specified
 * amount of time has passed
 */
waitable_t* after(int64_t microseconds);

void waitable_self_test();
