#pragma once

#include "types.h"

#include <util/except.h>

#include <stddef.h>

extern System_Type* typeof_System_Reflection_Assembly;
extern System_Type* typeof_System_Reflection_FieldInfo;
extern System_Type* typeof_System_Reflection_Module;
extern System_Type* typeof_System_Array;
extern System_Type* typeof_System_Type;
extern System_Type* typeof_System_String;

/**
 * The corelib assembly
 */
extern System_Reflection_Assembly* g_corelib;

/**
 * Initialize all the base types for dotnet
 */
err_t load_assembly_from_memory(System_Reflection_Assembly** out_assembly, void* file, size_t file_size);
