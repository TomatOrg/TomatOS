#pragma once

#include "spinlock.h"

#include <threading/thread.h>

typedef struct semaphore {
    uint32_t value;
    spinlock_t lock;
    waiting_thread_t* waiters;
    uint32_t nwait;
} semaphore_t;

void semaphore_acquire(semaphore_t* semaphore, bool lifo);

void semaphore_release(semaphore_t* semaphore, bool handoff);
