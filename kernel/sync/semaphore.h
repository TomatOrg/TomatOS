#pragma once

#include <stdint.h>

#include "condvar.h"
#include "mutex.h"

typedef struct semaphore {
    uint64_t value;
    mutex_t mutex;
    condvar_t condition;
} semaphore_t;

#define INIT_SEMAPHORE()  ((semaphore_t){ .value = 0, .mutex = INIT_MUTEX(), .condition = INIT_CONDVAR() })

void semaphore_signal(semaphore_t* semaphore);

bool semaphore_wait_until(semaphore_t* semaphore, uint64_t deadline);

void semaphore_reset(semaphore_t* semaphore);
