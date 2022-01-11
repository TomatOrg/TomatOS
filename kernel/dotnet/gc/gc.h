#pragma once

#include <dotnet/type.h>

void* gc_alloc(type_t type);

void* gc_alloc_array(type_t type, size_t size);
