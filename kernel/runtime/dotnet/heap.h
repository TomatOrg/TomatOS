#pragma once

#include <dotnet/types.h>

#include <stddef.h>

err_t init_heap();

void heap_dump_mapping();

/**
 * Allocate a new object
 *
 * @param size      [IN] The requested size
 * @param color     [IN] The color to give the object
 */
System_Object heap_alloc(size_t size, int color);

/**
 * Free an allocated object
 *
 * @param object    [IN] The object to free
 */
void heap_free(System_Object object);

/**
 * Reclaim heap memory, should be done after alot of object freeing
 */
void heap_reclaim();

/**
 * Find the object from a pointer, returns NULL if it is
 * not a real object
 */
System_Object heap_find(uintptr_t ptr);

/**
 * Same as heap_find but does not check if the object is mapped
 * or not, and assumes that if it is in the range it is mapped
 */
System_Object heap_find_fast(void* ptr);

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

/**
 * Dump the whole heap
 */
void heap_dump();
