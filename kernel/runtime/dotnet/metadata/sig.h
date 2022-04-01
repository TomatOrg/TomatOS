#pragma once

#include <runtime/dotnet/types.h>

#include <util/except.h>

err_t parse_compressed_integer(blob_entry_t* sig, uint32_t* value);

err_t parse_field_sig(blob_entry_t sig, System_Reflection_FieldInfo field);
