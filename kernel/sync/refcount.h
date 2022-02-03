#pragma once

#include <stdatomic.h>
#include <stdbool.h>

typedef struct refcount {
    int ref_count;
} refcount_t;

#define INIT_REFCOUNT() ((refcount_t){ 1 })

void refcount_inc(refcount_t* refcount);

bool refcount_dec(refcount_t* refcount);

bool refcount_is_one(refcount_t* refcount);
