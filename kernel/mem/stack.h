#pragma

/**
 * Allocate a new stack
 *
 * @remark
 * This returns the bottom of the stack and not the base of the stack
 */
void* alloc_stack();

/**
 * Free an allocated stack
 *
 * @param stack [IN] The bottom of the stack
 */
void free_stack(void* stack);
