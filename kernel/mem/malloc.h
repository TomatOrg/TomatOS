#pragma once

#include <util/except.h>

#include <stddef.h>

err_t init_malloc();

void* malloc(size_t size);

void* realloc(void* ptr, size_t size);

void free(void* ptr);

#define FREE(ptr) \
    do { \
        if (ptr != NULL) { \
            free(ptr); \
            ptr = NULL; \
        } \
    } while (0)
