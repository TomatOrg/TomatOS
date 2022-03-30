#pragma once

#include "mutex.h"

typedef struct rwmutex {
    mutex_t mutex;
    semaphore_t writer_sem;
    semaphore_t reader_sem;
    _Atomic(int32_t) reader_count;
    _Atomic(int32_t) reader_wait;
} rwmutex_t;

void rwmutex_rlock(rwmutex_t* rw);

void rwmutex_runlock(rwmutex_t* rw);

void rwmutex_lock(rwmutex_t* rw);

void rwmutex_unlock(rwmutex_t* rw);
