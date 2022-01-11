#pragma once

#include <stddef.h>
#include <uchar.h>

typedef struct system_string {
    int32_t length;
    const wchar_t data[];
} system_string_t;
