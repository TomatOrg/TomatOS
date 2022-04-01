#pragma once

#include <stdint.h>

typedef struct gc_object gc_object_t;

struct gc_object {
    // TODO: information about pointers in the object
    void* lol;

    // next free object in the chunk
    gc_object_t* next;

    // next chunk
    gc_object_t* chunk_next;

    // the log pointer, used to snapshot the object
    gc_object_t** log_pointer;

    // the color of the object, black and white switch during collection
    // and blue means unallocated
    uint8_t color;

    // the rank of the object from the allocator
    uint8_t rank;

    // For future use
    uint8_t _reserved[6];
};
