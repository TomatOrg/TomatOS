#pragma once

#include <runtime/dotnet/types.h>

#include <util/except.h>

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * Blue color is used to indicate
 * unallocated objects
 */
#define GC_COLOR_BLUE 2

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
void* gc_new(System_Type type, size_t size);

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
void gc_update(void* o, size_t offset, void* new);

/**
 * Update a pointer on the heap
 *
 * This is a wrapper around gc_update that takes a field name instead of raw offset
 */
#define GC_UPDATE(o, field, new) \
    do { \
        typeof(o) _o = o; \
        gc_update(_o, offsetof(typeof(*(_o)), field), new); \
    } while (0)

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
