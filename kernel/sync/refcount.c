#include "refcount.h"


void refcount_inc(refcount_t* refcount) {
    atomic_fetch_add_explicit(&refcount->ref_count, 1, memory_order_relaxed);
}

bool refcount_dec(refcount_t* refcount) {
    return atomic_fetch_sub_explicit(&refcount->ref_count, 1, memory_order_acq_rel) != 0;
}

bool refcount_is_one(refcount_t* refcount) {
    return atomic_load_explicit(&refcount->ref_count, memory_order_acquire) == 1;
}
