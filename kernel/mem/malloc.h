#pragma once

#include <util/except.h>

#include <stddef.h>

err_t init_malloc();

void check_malloc();

void* malloc(size_t size);

void* malloc_aligned(size_t size, size_t alignment);

void* realloc(void* ptr, size_t size);

void free(void* ptr);

#define FREE(ptr) \
    do { \
        if (ptr != NULL) { \
            free(ptr); \
            ptr = NULL; \
        } \
    } while (0)
