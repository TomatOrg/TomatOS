#pragma once

#include "types.h"

/**
 * Create a new string from UTF8 string
 *
 * @param str   [IN] The input string
 * @param len   [IN] The input string length (in bytes)
 */
System_String new_string_from_utf8(const char* str, size_t len);

System_String new_string_from_cstr(const char* str);
