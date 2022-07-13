#pragma once

#include <util/except.h>

#include <stddef.h>

err_t init_malloc();

void check_malloc();

void* malloc(size_t size) __attribute__((alloc_size(1)));

void* malloc_aligned(size_t size, size_t alignment) __attribute__((alloc_size(1), alloc_align(2)));

void* realloc(void* ptr, size_t size) __attribute__((alloc_size(2)));

void free(void* ptr);

#define SAFE_FREE(ptr) \
    do { \
        if (ptr != NULL) { \
            free(ptr); \
            ptr = NULL; \
        } \
    } while (0)
