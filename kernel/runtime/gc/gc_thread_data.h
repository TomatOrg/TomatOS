#pragma once

#include "object.h"

#include <stdbool.h>
#include <stdint.h>

typedef union object_set_entry {
    gc_object_t* key;
    gc_object_t* value;
} object_set_entry_t;

typedef object_set_entry_t* object_set_t;

typedef struct gc_thread_data {
    /**
     * Is tracing set on
     */
    bool trace_on;

    /**
     * Is snooping set on
     */
    bool snoop;

    /**
     * The color used to allocate objects
     */
    uint8_t alloc_color;

    /**
     * The tracing buffer of the thread
     */
    gc_object_t** buffer;

    /**
     * The snooped object object set
     */
    object_set_t snooped;
} gc_thread_data_t;

/**
 * The default gc thread data, used when creating new threads
 */
extern gc_thread_data_t m_default_gc_thread_data;
