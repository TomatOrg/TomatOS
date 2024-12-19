#pragma once

#include <stddef.h>

#define memset __builtin_memset
#define memcpy __builtin_memcpy
#define memmove __builtin_memmove
#define memcmp __builtin_memcmp

#define strlen __builtin_strlen
#define strcmp __builtin_strcmp

void string_verify_features(void);
