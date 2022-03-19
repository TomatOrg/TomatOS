#pragma once

#include "types.h"

/**
 * Request an object of the given size from the heap
 */
System_Object* heap_alloc(size_t size);

/**
 * Return an object to the heap
 */
void heap_free(System_Object* object);

/**
 * Flush all freed objects, this will will possibly create small chunks
 */
void heap_flush();
