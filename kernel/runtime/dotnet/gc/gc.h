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
 * Green color is used to indicate
 * an root object that should be
 * finalized
 */
#define GC_COLOR_GREEN 3

/**
 * Initialize the garbage collector
 */
err_t init_gc();

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Allocate objects and update pointers
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Add a root to the gc, required for proper garbage collection
 *
 * @param object    [IN] The object to add
 */
void gc_add_root(void* object);

/**
 * Allocate a new object from the garbage collector of the given type and of
 * the given size
 *
 * @param type      [IN] The type of the object
 * @param count     [IN] The size to allocate
 */
void* gc_new(System_Type type, size_t size);

/**
 * Helper to allocate a new gc object
 */
#define GC_NEW(type) \
    ({ \
        System_Type __type = type; \
        gc_new(__type, __type->ManagedSize); \
    })

#define GC_NEW_STRING(count) \
    ({ \
        size_t __count = count; \
        gc_new(tSystem_String, tSystem_String->ManagedSize + 2 * __count); \
    })

/**
 * Helper to allocate a new array
 */
#define GC_NEW_ARRAY(elementType, count) \
    ({ \
        size_t __count = count; \
        System_Type __elementType = elementType; \
        System_Type __arrayType = get_array_type(__elementType); \
        System_Array __newArray = gc_new(__arrayType, __arrayType->ManagedSize + __elementType->StackSize * __count); \
        __newArray->Length = __count; \
        (void*)__newArray; \
    })

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
        __typeof(o) _o = o; \
        gc_update(_o, offsetof(__typeof(*(_o)), field), new); \
    } while (0)

#define GC_UPDATE_ARRAY(o, idx, new) \
    do { \
        __typeof(o) _o = o; \
        gc_update(_o, offsetof(__typeof(*(_o)), Data) + (idx) * sizeof(__typeof(_o->Data[0])), new); \
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
