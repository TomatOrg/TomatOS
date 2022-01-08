#pragma once

#include "dotnet.h"

#include <stdbool.h>

typedef enum field_attributes {
    FIELD_ATTRIBUTES_STATIC = 0x0010
} field_attributes_t;

struct field_info {
    assembly_t assembly;
    const char* name;
    type_t declaring_type;
    token_t metadata_token;

    // The attributes associated with this field.
    uint32_t attributes;

    // The type of this field object.
    type_t field_type;

    // The offset of the field in memory, only needed for
    // non-static fields
    int offset;
};

static inline bool field_is_static(field_info_t field_info) { return field_info->attributes & FIELD_ATTRIBUTES_STATIC; }
