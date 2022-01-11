#include "gc.h"

#include <mem/malloc.h>

void* gc_alloc(type_t type) {
    return malloc(type->managed_size);
}

void* gc_alloc_array(type_t type, size_t size) {
    return malloc(sizeof(int32_t) + type->stack_size * size);
}
