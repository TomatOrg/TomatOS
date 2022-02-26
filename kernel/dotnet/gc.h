#pragma once

#include "types.h"
#include "util/except.h"

#include <stdbool.h>
#include <stdint.h>

typedef union object_set_entry {
    object_t* key;
    object_t* value;
} object_set_entry_t;

typedef object_set_entry_t* object_set_t;

typedef struct gc_local_data {
    bool trace_on;
    bool snoop;
    uint8_t alloc_color;
    object_t** buffer;
    object_set_t snooped;
} gc_local_data_t;

/**
 * Blue color is used to indicate
 * unallocated objects
 */
#define COLOR_BLUE 2

/**
 * Initialize the garbage collector
 */
err_t init_gc();

/**
 * Allocate a new object from the garbage collector of the given type and of
 * the given size
 *
 * @param type      [IN] The type of the object
 * @param count     [IN] The size to allocate
 */
object_t* gc_new(type_t* type, size_t size);

/**
 * Update a pointer on the heap
 *
 * @remark
 * This must take an object that is allocated on the heap, it should not be used for local
 * pointers on the stack or for global variables.
 *
 * TODO: maybe just give the field info
 *
 * @param o         [IN] The object we are updating
 * @param offset    [IN] The offset of the field to update
 * @param new       [IN] The new object we are updating
 */
void gc_update(object_t* o, size_t offset, object_t* new);

/**
 * Trigger the collection in an async manner
 */
void gc_wake();

/**
 * Trigger the gc and wait for it to finish
 */
void gc_wait();
