#pragma once

#include "dotnet.h"

struct parameter_info {
    assembly_t assembly;
    const char* name;
    method_info_t declaring_method;
    uint32_t attributes;
    type_t parameter_type;
    int position;
};
