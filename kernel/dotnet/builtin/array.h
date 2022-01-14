#pragma once

#include <stddef.h>
#include <stdint.h>
#include <util/except.h>

typedef struct array {
    int32_t size;
    uint8_t data[];
} system_array_t;
