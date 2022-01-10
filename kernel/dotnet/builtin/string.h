#pragma once

#include <stddef.h>

typedef struct system_string {
    size_t length;
    const char* data;
} system_string_t;

system_string_t* system_string_from_cstr(const char* data);
