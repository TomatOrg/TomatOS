#pragma once

#include <stddef.h>
#include <lib/string.h>

void init_alloc(void);

void* mem_alloc(size_t size);

void* mem_realloc(void* ptr, size_t size);

void mem_free(void* ptr);
