#pragma once

#include <dotnet/dotnet.h>
#include <util/except.h>

#include <stdint.h>

typedef struct metadata_table {
    void* table;
    uint32_t rows;
} metadata_table_t;

typedef struct metadata {
    metadata_table_t tables[64];
} metadata_t;

/**
 * Parse all the metadata stream into the metadata structure organized in nice addressable tables
 *
 * @param assembly  [IN]    The assembly the metadata is related to
 * @param stream    [IN]    The stream to parse
 * @param size      [IN]    The size of the stream to parse
 * @param metadata  [OUT]   The metadata output
 * @return
 */
err_t metadata_parse(assembly_t assembly, void* stream, size_t size, metadata_t* metadata);

/**
 * Free all the allocated metadata
 *
 * @param metadata  [IN]    The allocated metadata
 */
void free_metadata(metadata_t* metadata);
