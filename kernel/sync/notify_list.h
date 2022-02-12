#pragma once

#include "spinlock.h"

#include <threading/thread.h>

#include <stdint.h>

typedef struct notify_list {
    // the ticket number of the next waiter. It is atomically
    // incremented outside the lock
    uint32_t wait;

    // the ticket number of the next waiter to be notified. It can
    // be read outside the lock, but is only written with the lock held.
    // both wait and notify can wrap around, and such cases will be
    // correctly handled as long as their "wrapped" difference is bounded by 2^31.
    uint32_t notify;

    // list of parked waiters
    spinlock_t lock;
    waiting_thread_t* head;
    waiting_thread_t* tail;
} notify_list_t;

uint32_t notify_list_add(notify_list_t* list);

void notify_list_wait(notify_list_t* list, uint32_t ticket);

void notify_list_notify_all(notify_list_t* list);

void notify_list_notify_one(notify_list_t* list);
