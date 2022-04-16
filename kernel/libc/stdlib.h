#pragma once

#include <mem/malloc.h>

_Noreturn static inline void exit(int status) {
    ASSERT(!"libc exit was called");
}

void qsort(void* base,size_t nmemb,size_t size,int (*compar)(const void*,const void*));

#define alloca __builtin_alloca
