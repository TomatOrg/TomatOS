#pragma once

#include <util/except.h>
#include <dotnet/dotnet.h>
#include <mir/mir.h>

typedef struct jitter_method_info {
    // info the jit needs
    MIR_item_t proto;
    MIR_item_t forward;
} jitter_method_info_t;

err_t jitter_jit_assembly(assembly_t assembly);
