#pragma once

#include "pe_spec.h"

#include <dotnet/metadata/metadata.h>
#include <dotnet/assembly.h>

#include <stdint.h>
#include <stddef.h>

typedef struct pe_context {
    // the binary itself
    const uint8_t* blob;
    size_t blob_size;

    // the sections
    pe_section_header_t* section_headers;
    size_t section_header_count;

    // the assembly we are loading
    assembly_t assembly;

    // temporary metadata
    pe_cli_header_t* cli_header;
    metadata_t metadata;
} parsed_pe_t;

/**
 * Parse a PE file and get from it the cli header and
 * metadata for further loading, this also contains the
 * sections for further inspection
 *
 * @remark
 * The blob, blob_size and assembly fields must be set prior
 * to loading of the PE.
 *
 * @param ctx   [IN] The output
 */
err_t pe_parse(parsed_pe_t* ctx);

/**
 * Get RVA data from the parsed pe
 *
 * @remark
 * The returned pointer must be freed with `free`
 *
 * @param ctx           [IN] The parsed PE
 * @param directory     [IN] The directory (must contain valid size)
 */
void* pe_get_rva_data(parsed_pe_t* ctx, pe_directory_t directory);

/**
 * Get a pointer to the start of the RVA in memory
 *
 * @remark
 * The pointer points to memory allocated by the parsed pe, once `free_parsed_pe`
 * is called, they will be invalidated!
 *
 * @param ctx           [IN] The parsed PE
 * @param directory     [IN] The directory (remaining size in the section will be set)
 */
const void* pe_get_rva_ptr(parsed_pe_t* ctx, pe_directory_t* directory);

/**
 * Free the parsed PE and all of its data
 *
 * @param ctx   [IN] The parsed PE
 */
void free_parsed_pe(parsed_pe_t* ctx);
