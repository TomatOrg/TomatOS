#pragma once

#include "types.h"

#include "util/except.h"

/**
 * The core library instance
 */
extern System_Reflection_Assembly g_corelib;

/**
 * Loading the corelib itself
 *
 * @param buffer        [IN] The corelib binary
 * @param buffer_size   [IN] The corelib binary size
 */
err_t loader_load_corelib(void* buffer, size_t buffer_size);

/**
 * Fill the type information of the given type
 */
err_t loader_fill_type(System_Type type, System_Type_Array genericTypeArguments, System_Type_Array genericMethodArguments);

/**
 * Fill the method information of the given method
 */
err_t loader_fill_method(System_Type type, System_Reflection_MethodInfo method, System_Type_Array genericTypeArguments, System_Type_Array genericMethodArguments);
