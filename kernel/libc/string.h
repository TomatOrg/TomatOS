#pragma once

#include <stddef.h>

#include <util/string.h>

// MIR needs the exact memcpy reference sadly
#undef memcpy
void* memcpy(void* restrict dest, const void* restrict src, size_t n);
void* memchr(const void *src, int c, size_t n);
static inline char *strerror(int errnum) { return ""; }