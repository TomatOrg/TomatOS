#pragma once

#include <stdint.h>
#include <stivale2.h>

/**
 * Get a stivale2 tag
 *
 * @param tag_id    [IN] The tag to find
 */
void* get_stivale2_tag(uint64_t tag_id);

/**
 * Get a stivale2 module
 *
 * @param name      [IN] The name of the module we want
 */
struct stivale2_module* get_stivale2_module(const char* name);
