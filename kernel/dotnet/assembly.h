#pragma once

#include <util/except.h>
#include <uchar.h>

#include "dotnet.h"

struct assembly {
    // unique identifiers
    const char* name;
    guid_t guid;

    // types, fields and methods
    struct type* types;
    int types_count;
    struct method_info* methods;
    int methods_count;
    struct field_info* fields;
    int fields_count;

    // The entry point method
    method_info_t entry_point;

    // static data, all allocated on the heap
    char* strings;
    size_t strings_size;
    uint8_t* us;
    size_t us_size;
    uint8_t* blob;
    size_t blob_size;
    guid_t* guids;
    size_t guids_count;
};

/**
 * The default corlib
 */
extern assembly_t g_corlib;

/**
 * Load an assembly from the given blob
 *
 * The blob should essentially be a .NET EXE/DLL
 *
 * @param assembly          [OUT] The new assembly
 * @param buffer            [IN] The blob with the assembly
 * @param buffer_size       [IN] The size of the blob
 */
err_t load_assembly_from_blob(assembly_t* assembly, const void* buffer, size_t buffer_size);

/**
 * Get a type by its token from the given assembly, the token can be for either type def,
 * type ref or a type spec.
 *
 * @param assembly          [IN] The assembly the token belonds to
 * @param token             [IN] The token to search for
 */
type_t assembly_get_type_by_token(assembly_t assembly, token_t token);

/**
 *
 * @param assembly
 * @param token
 * @return
 */
method_info_t assembly_get_method_info_by_token(assembly_t assembly, token_t token);

/**
 *
 * @param assembly
 * @param token
 * @return
 */
field_info_t assembly_get_field_info_by_token(assembly_t assembly, token_t token);
