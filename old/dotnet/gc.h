#pragma once

#include "types.h"

#include <util/except.h>

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

typedef union object_set_entry {
    System_Object* key;
    System_Object* value;
} object_set_entry_t;

typedef object_set_entry_t* object_set_t;

typedef struct stack_frame {
    // the previous stack frame
    struct stack_frame* prev;

    // the amount of pointers on this frame
    size_t count;

    // the pointers
    System_Object* pointers[];
} stack_frame_t;

typedef struct gc_local_data {
    bool trace_on;
    bool snoop;
    uint8_t alloc_color;
    System_Object** buffer;
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Allocate objects and update pointers
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Allocate a new object from the garbage collector of the given type and of
 * the given size
 *
 * @param type      [IN] The type of the object
 * @param count     [IN] The size to allocate
 */
void* gc_new(System_Type* type, size_t size);

/**
 * Utility to allocate a normal type
 */
#define GC_NEW(type) gc_new(typeof_##type, sizeof(gc_new))

/**
 * Utility to allocate an array type
 */
#define GC_NEW_ARRAY(type, count) gc_new(get_array_type(typeof_##type), SYSTEM_ARRAY_SIZE(type, count))

/**
 * Utility to allocate an array that contains references to objects
 */
#define GC_NEW_REF_ARRAY(type, count) gc_new(get_array_type(typeof_##type), SYSTEM_ARRAY_SIZE(type*, count))

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
void gc_update(System_Object* o, size_t offset, System_Object* new);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Triggering the garbage collector
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Trigger the collection in an async manner
 */
void gc_wake();

/**
 * Trigger the gc and wait for it to finish
 */
void gc_wait();
