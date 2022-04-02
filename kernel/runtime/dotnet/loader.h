#pragma once

#include <stddef.h>
#include "util/except.h"
#include "types.h"

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
