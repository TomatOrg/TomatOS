#pragma once

#include <runtime/dotnet/types.h>
#include <util/except.h>

/**
 * Initialize the jit itself
 */
err_t init_jit();

/**
 * Jit an assembly to the global context
 */
err_t jit_assembly(System_Reflection_Assembly assembly);

/**
 * Dump the MIR of the given method
 */
void jit_dump_mir(System_Reflection_MethodInfo methodInfo);

typedef struct method_result {
    System_Exception exception;
    uintptr_t value;
} method_result_t;
