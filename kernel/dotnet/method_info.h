#pragma once

#include "dotnet.h"

struct method_info {
    assembly_t assembly;
    const char* name;
    type_t declaring_type;
    token_t metadata_token;

    /**
     * The attributes associated with this method.
     */
    uint32_t attributes;

    struct parameter_info* parameters;
    size_t parameters_count;

    type_t return_type;

    uint8_t* il;
    int il_size;
    int max_stack_size;

    // TODO: local variables

    // TODO: exceptions
};
