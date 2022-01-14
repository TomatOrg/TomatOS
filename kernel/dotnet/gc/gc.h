#pragma once

#include <dotnet/type.h>

typedef struct gc_header {
    // the type of the allocated object
    type_t type;
} gc_header_t;

void* gc_alloc(type_t type);

void* gc_alloc_array(type_t type, size_t size);
