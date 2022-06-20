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
} waitable_t;

typedef enum waitable_result {
    WAITABLE_EMPTY,
    WAITABLE_CLOSED,
    WAITABLE_SUCCESS,
} waitable_result_t;

/**
 * Create a new waitable of the given size
 */
waitable_t* create_waitable(size_t size);

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

/**
 * Waits on all the given waitables and waits for one of them
 * to be ready
 */
int waitable_select(waitable_t** waitables, int send_count, int wait_count, bool block);
