#pragma once

#include <stddef.h>
#include <lib/string.h>

/**
 * Simple allocator meant for simple memory management in the kernel
 * this is not designed for performance but for simplicity
 *
 * Interrupt safe
 */
void* mem_alloc(size_t size);

void* mem_realloc(void* ptr, size_t size);

void mem_free(void* ptr);
