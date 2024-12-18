#pragma once

/**
 * Allocates a small 32kb large stack, should be
 * used for tasks and scheduling
 */
void* small_stack_alloc(void);
