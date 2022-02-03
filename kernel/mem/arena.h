#pragma once

#include <stddef.h>

typedef struct arena arena_t;

/**
 * Create a new arena allocator
 */
arena_t* create_arena();

/**
 * Allocate data from the arena
 *
 * @param arena     [IN] The arena to allocate from
 * @param size      [IN] The size to allocate
 */
void* arena_alloc(arena_t* arena, size_t size);

/**
 * Free the arena and all the blocks associated with it
 *
 * @param arena     [IN] The arena to free
 */
void free_arena(arena_t* arena);

#define FREE_ARENA(arena) \
    do { \
        if (arena != NULL) { \
            free_arena(arena); \
            arena = NULL; \
        } \
    } while (0)
