#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct rwlock {
    size_t lock;
} rwlock_t;

void rwlock_read(rwlock_t* rwlock);

void rwlock_write(rwlock_t* rwlock);

bool rwlock_try_read(rwlock_t* rwlock);

bool rwlock_try_write(rwlock_t* rwlock);

void rwlock_read_unlock(rwlock_t* rwlock);

void rwlock_write_unlock(rwlock_t* rwlock);
