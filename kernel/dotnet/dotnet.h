#pragma once

#include <util/defs.h>

#include <stdint.h>

// forward declare all the structs
struct type;
struct method_info;
struct field_info;
struct parameter_info;
struct assembly;

typedef struct type* type_t;
typedef struct method_info* method_info_t;
typedef struct field_info* field_info_t;
typedef struct parameter_info* parameter_info_t;
typedef struct assembly* assembly_t;

/**
 * Represents a guid
 */
typedef struct guid {
    uint64_t low;
    uint64_t high;
} PACKED guid_t;

/**
 * Represents a token that can be used for looking up
 * stuff in the dotnet assembly
 */
typedef union token {
    struct {
        uint32_t index : 24;
        uint32_t table : 8;
    };
    uint32_t packed;
} PACKED token_t;
