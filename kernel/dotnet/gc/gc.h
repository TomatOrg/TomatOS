#pragma once

#include <dotnet/type.h>

void init_gc();

void* gc_alloc(type_t type);
