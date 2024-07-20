#pragma once
#include <stddef.h>

/**
 * Simple allocator meant for simple memory management in the kernel
 * this is not designed for performance but for simplicity
 *
 * Limited to up to 4KB allocations
 *
 * Interrupt safe
 */
void* mem_alloc(size_t size);

void mem_free(void* ptr);
