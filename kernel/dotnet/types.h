#pragma once

#include <dotnet/metadata/metadata_spec.h>

#include <util/except.h>

#include "type.h"

extern type_t g_void;

extern type_t g_sbyte;
extern type_t g_byte;
extern type_t g_short;
extern type_t g_ushort;
extern type_t g_int;
extern type_t g_uint;
extern type_t g_long;
extern type_t g_ulong;
extern type_t g_nint;
extern type_t g_nuint;

extern type_t g_float;
extern type_t g_double;

extern type_t g_bool;
extern type_t g_char;

extern type_t g_string;
extern type_t g_object;
extern type_t g_value_type;
extern type_t g_array;

/**
 * Initialize all the global base types from the loaded
 * core library
 */
err_t initialize_base_types(metadata_type_def_t* base_types, int base_types_count);
