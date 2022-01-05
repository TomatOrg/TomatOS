#pragma once

#include <stddef.h>

#define memset __builtin_memset
#define memcpy __builtin_memcpy
#define memcmp __builtin_memcmp
#define memmove __builtin_memmove

#define strncmp __builtin_strncmp
#define strcmp __builtin_strcmp
#define strcpy __builtin_strcpy
#define strlen __builtin_strlen

unsigned long int strtoul(const char *nptr, char **endptr, int base);
