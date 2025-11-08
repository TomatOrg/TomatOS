#pragma once

#include "../debug/log.h"
#include "cpp_magic.h"

#define ASSERT(expr, ...) \
    do { \
        if (!(expr)) { \
            IF(HAS_ARGS(__VA_ARGS__))(ERROR(__VA_ARGS__)); \
            ERROR("Assertion failed at %s (%s:%d)", __FUNCTION__, __FILE__, __LINE__); \
            __builtin_trap(); \
        } \
    } while (0)
