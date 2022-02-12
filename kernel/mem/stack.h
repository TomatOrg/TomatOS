#pragma

#define STACK_SIZE SIZE_2MB

/**
 * Allocate a new stack stack buffer, it has guard pages and is 2MB in size
 */
void* alloc_stack();

/**
 * Free an allocated stack
 */
void free_stack(void* stack);
