#pragma once

#include "pe_spec.h"

#include <util/except.h>

#include <stdint.h>
#include <stddef.h>

typedef struct pe_file {
    // the binary itself
    const uint8_t* file;
    size_t file_size;

    // the sections
    pe_section_header_t* section_headers;
    size_t section_header_count;

    // specific parts in the loaded assembly
    char* strings;
    size_t strings_size;
    uint8_t* us;
    size_t us_size;
    uint8_t* blob;
    size_t blob_size;
    guid_t* guids;
    size_t guids_count;

    // temporary metadata
    pe_cli_header_t* cli_header;
} pe_file_t;

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
err_t pe_parse(pe_file_t* ctx);

/**
 * Get RVA data from the parsed pe
 *
 * @remark
 * The returned pointer must be freed with `free`
 *
 * @param ctx           [IN] The parsed PE
 * @param directory     [IN] The directory (must contain valid size)
 */
void* pe_get_rva_data(pe_file_t* ctx, pe_directory_t directory);

/**
 * Get a pointer to the start of the RVA in memory
 *
 * @remark
 * The pointer points to memory allocated by the parsed pe, once `free_pe_file`
 * is called, they will be invalidated!
 *
 * @param ctx           [IN] The parsed PE
 * @param directory     [IN] The directory (remaining size in the section will be set)
 */
const void* pe_get_rva_ptr(pe_file_t* ctx, pe_directory_t* directory);

/**
 * Free the parsed PE and all of its data
 *
 * @param ctx   [IN] The parsed PE
 */
void free_pe_file(pe_file_t* ctx);
