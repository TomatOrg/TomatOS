#pragma once

#include "types.h"

/**
 * Request an object of the given size from the heap
 */
object_t* heap_alloc(size_t size);

/**
 * Return an object to the heap
 */
void heap_free(object_t* object);

/**
 * Flush all freed objects, this will will possibly create small chunks
 */
void heap_flush();
