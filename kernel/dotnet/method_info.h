#pragma once

#include <util/buffer.h>
#include <dotnet/jit/jitter.h>
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

    // jit specific data
    jitter_method_info_t jit;
};

void method_signature_string(method_info_t info, buffer_t* buffer);

void method_full_name(method_info_t info, buffer_t* buffer);
