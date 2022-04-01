#pragma once

#include <stddef.h>
#include "util/except.h"

/**
 * Loading the corelib itself
 *
 * @param buffer        [IN] The corelib binary
 * @param buffer_size   [IN] The corelib binary size
 */
err_t loader_load_corelib(void* buffer, size_t buffer_size);
