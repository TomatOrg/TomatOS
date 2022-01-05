#pragma once

#include "dotnet.h"

struct assembly {
    // unique identifiers
    const char* name;
    guid_t guid;

    // types, fields and methods
    struct type* types;
    int types_count;
    struct method_info* methods;
    int methods_count;
    struct field_info* fields;
    int fields_count;

    struct method_info* entry_point;

    // some other information
    size_t stack_commit_size;
    size_t stack_reserve_size;
//    size_t heap_commit_size;
//    size_t heap_reserve_size;

    // static data, all allocated on the heap
    char* strings;
    size_t strings_size;
    char* us;
    size_t us_size;
    uint8_t* blob;
    size_t blob_size;
    guid_t* guids;
    size_t guids_count;
};
