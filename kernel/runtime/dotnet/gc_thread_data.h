#pragma once

struct System_Object;

#include <stdbool.h>
#include <stdint.h>

typedef union object_set_entry {
    struct System_Object* key;
    struct System_Object* value;
} object_set_entry_t;

typedef object_set_entry_t* object_set_t;

typedef enum gc_thread_status {
    THREAD_STATUS_ASYNC,
    THREAD_STATUS_SYNC1,
    THREAD_STATUS_SYNC2,
} gc_thread_status_t;

typedef struct gc_thread_data {
    gc_thread_status_t status;
} gc_thread_data_t;

/**
 * The default gc thread data, used when creating new threads
 */
extern gc_thread_data_t m_default_gc_thread_data;
