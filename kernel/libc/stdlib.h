#pragma once

#include <mem/malloc.h>

_Noreturn static inline void exit(int status) {
    ASSERT(!"libc exit was called");
}

_Noreturn static inline void abort() {
    ASSERT(!"libc abort was called");
}

void qsort(void* base,size_t nmemb,size_t size,int (*compar)(const void*,const void*));

#define alloca __builtin_alloca

// mimalloc uses them
static inline char *realpath(const char *restrict path, char *restrict resolved_path) {
    return NULL;
}
static inline int atexit(void (*function)(void)) {
    return 0;
}