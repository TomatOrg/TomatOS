#pragma once

#include "mutex.h"
#include "condition.h"

typedef struct rwmutex {
    mutex_t mutex;
    condition_t condition;

    bool is_write_locked;
    unsigned num_readers;
    unsigned num_waiting_writers;
} rwmutex_t;

void rwmutex_read_lock(rwmutex_t* rwmutex);
void rwmutex_read_unlock(rwmutex_t* rwmutex);

void rwmutex_write_lock(rwmutex_t* rwmutex);
void rwmutex_write_unlock(rwmutex_t* rwmutex);
