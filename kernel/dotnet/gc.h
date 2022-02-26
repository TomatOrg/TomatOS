#pragma once

#include "types.h"

#include <util/except.h>

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

typedef union object_set_entry {
    object_t* key;
    object_t* value;
} object_set_entry_t;

typedef object_set_entry_t* object_set_t;

typedef struct stack_frame {
    // the previous stack frame
    struct stack_frame* prev;

    // the amount of pointers on this frame
    size_t count;

    // the pointers
    object_t* pointers[];
} stack_frame_t;

typedef struct gc_local_data {
    bool trace_on;
    bool snoop;
    uint8_t alloc_color;
    object_t** buffer;
    object_set_t snooped;

    stack_frame_t* top_of_stack;
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
 * @remark
 * The pointer must be placed into a valid stack frame or global variable in order for the
 * allocation to be properly traced without any race
 *
 * @param type      [IN] The type of the object
 * @param count     [IN] The size to allocate
 * @param output    [OUT] The newly allocated object
 */
void gc_new(type_t* type, size_t size, object_t** output);

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Stack frame handling
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void stack_frame_cleanup(stack_frame_t** ptr);

void stack_frame_push(stack_frame_t* frame);

/**
 * Declare the stack frame for the current function
 */
#define STACK_FRAME(__count) \
    __attribute__((cleanup(stack_frame_cleanup))) stack_frame_t* frame = __builtin_alloca(sizeof(stack_frame_t) + __count * sizeof(object_t*)); \
    __builtin_memset(frame, 0, sizeof(stack_frame_t) + __count * sizeof(object_t*)); \
    frame->count = __count; \
    stack_frame_push(frame); \

