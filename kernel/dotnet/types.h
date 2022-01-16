#pragma once

#include <dotnet/metadata/metadata_spec.h>

#include <util/except.h>

#include "type.h"

//----------------------------------------------------------------------------------------------------------------------
// Primitive types
//----------------------------------------------------------------------------------------------------------------------

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

//----------------------------------------------------------------------------------------------------------------------
// Base types
//----------------------------------------------------------------------------------------------------------------------
extern type_t g_string;
extern type_t g_object;
extern type_t g_value_type;
extern type_t g_array;

//----------------------------------------------------------------------------------------------------------------------
// Exception types that the runtime might throw
//----------------------------------------------------------------------------------------------------------------------
extern type_t g_arithmetic_exception;
extern type_t g_overflow_exception;
extern type_t g_null_reference_exception;
extern type_t g_divide_by_zero_exception;

/**
 * Initialize all the global base types from the loaded
 * core library
 */
err_t initialize_base_types(metadata_type_def_t* base_types, int base_types_count);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Type handling
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

type_t get_underlying_type(type_t T);

type_t get_reduced_type(type_t T);

type_t get_verification_type(type_t T);

type_t get_intermediate_type(type_t T);

type_t get_direct_base_class(type_t T);

bool is_type_compatible_with(type_t T, type_t U);

bool is_type_assignable_to(type_t from, type_t to);
