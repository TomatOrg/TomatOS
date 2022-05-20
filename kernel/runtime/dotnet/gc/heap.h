#pragma once

#include <runtime/dotnet/types.h>

#include <stddef.h>

err_t init_heap();

System_Object heap_alloc(size_t size);

/**
 * Find the object from a pointer, returns NULL if it is
 * not a real object
 */
System_Object heap_find(uintptr_t ptr);

typedef void (*object_callback_t)(System_Object object);

/**
 * Iterate all the dirty objects in the heap
 *
 * once iterated the card will be marked as clear
 */
void heap_iterate_dirty_objects(object_callback_t callback);

/**
 * Iterate all the objects on the heap, be it allocated or not, and call the
 * iven callback function
 */
void heap_iterate_objects(object_callback_t callback);