#pragma once

#include <stddef.h>
#include <stdint.h>
#include <util/except.h>

typedef struct array {
    // the size of the object
    int32_t size;

    // the raw data, allocated right after this...
    uint8_t data[];
} system_array_t;
