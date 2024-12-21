#pragma once

#include <stdatomic.h>

// the atomics are relaxed by default
#define atomic_read(ptr)            atomic_load_explicit(ptr, memory_order_relaxed)
#define atomic_write(ptr, value)    atomic_store_explicit(ptr, value, memory_order_relaxed)
#define atomic_add(ptr, value)      atomic_fetch_add_explicit(ptr, value, memory_order_relaxed)
#define atomic_sub(ptr, value)      atomic_fetch_sub_explicit(ptr, value, memory_order_relaxed)
#define atomic_and(ptr, value)      atomic_fetch_and_explicit(ptr, value, memory_order_relaxed)
#define atomic_or(ptr, value)       atomic_fetch_or_explicit(ptr, value, memory_order_relaxed)
