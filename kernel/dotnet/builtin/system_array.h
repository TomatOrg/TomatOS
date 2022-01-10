#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct system_array {
    // the size of the object
    size_t size;

    // the raw data, allocated right after this...
    uint8_t data[];
} system_array_t;
