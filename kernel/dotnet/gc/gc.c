#include "gc.h"

#include <sync/ticketlock.h>
#include <mem/malloc.h>

void* gc_alloc(type_t type) {
    return malloc(type->managed_size);
}

void init_gc() {

}
