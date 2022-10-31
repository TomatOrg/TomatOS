#pragma once

#include "spinlock.h"

#include <thread/thread.h>

typedef struct semaphore {
    _Atomic(uint32_t) value;
    spinlock_t lock;
    waiting_thread_t* waiters;
    _Atomic(uint32_t) nwait;
} semaphore_t;

bool semaphore_acquire(semaphore_t* semaphore, bool lifo, int64_t timeout);

void semaphore_release(semaphore_t* semaphore, bool handoff);

void semaphore_self_test();
