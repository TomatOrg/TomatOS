#pragma once

#include "mutex.h"
#include "condition.h"

typedef struct semaphore {
    mutex_t mutex;
    condition_t condition;
    int value;
} semaphore_t;

#define INIT_SEMAPHORE(val) (semaphore_t){ .mutex = INIT_MUTEX(), .condition = INIT_CONDITION(), .value = (val) }

void semaphore_signal(semaphore_t* semaphore);

bool semaphore_wait(semaphore_t* semaphore, int64_t timeout);
