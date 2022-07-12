#pragma once

#define STACK_SIZE SIZE_2MB

#define PUSH(type, stack, value) \
    ({ \
        stack -= sizeof(type); \
        *(type*)stack = (type)value; \
    })

#define PUSH64(stack, value) PUSH(uint64_t, stack, value)
#define PUSH32(stack, value) PUSH(uint32_t, stack, value)
#define PUSH16(stack, value) PUSH(uint16_t, stack, value)
#define PUSH8(stack, value) PUSH(uint8_t, stack, value)

/**
 * Allocate a new stack stack buffer, it has guard pages and is 2MB in size
 */
void* alloc_stack();

/**
 * Free an allocated stack
 */
void free_stack(void* stack);
