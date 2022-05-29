#pragma once
#include "stb_ds.h"
#include <stdio.h>

typedef struct {
    char* buf;
} strbuilder_t;

// "allocate" a stringbuilder
strbuilder_t strbuilder_new();

// free a stringbuilder
void strbuilder_free(strbuilder_t* builder);

// append a UTF16 string at the end, with explicit length
void strbuilder_utf16(strbuilder_t* builder, const __CHAR16_TYPE__* str, size_t length);

// append a zero-terminated UTF8 string
void strbuilder_cstr(strbuilder_t* builder, const char* str);

// append a single ASCII character
void strbuilder_char(strbuilder_t* builder, char c);

// append a size_t
void strbuilder_uint(strbuilder_t* builder, size_t n);

// get byte data
char* strbuilder_get(strbuilder_t* builder);