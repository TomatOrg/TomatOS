#pragma once

#include "pe.h"

#include <util/except.h>

#include <stdint.h>

typedef struct metadata_table {
    void* table;
    int rows;
} metadata_table_t;

typedef struct metadata {
    metadata_table_t tables[64];
} metadata_t;

static inline metadata_type_def_t* metadata_get_type_def(metadata_t* metadata, int index) {
    ASSERT(index < metadata->tables[METADATA_TYPE_DEF].rows);
    return &((metadata_type_def_t*)metadata->tables[METADATA_TYPE_DEF].table)[index];
}

static inline metadata_method_def_t* metadata_get_method_def(metadata_t* metadata, int index) {
    ASSERT(index < metadata->tables[METADATA_METHOD_DEF].rows);
    return &((metadata_method_def_t*)metadata->tables[METADATA_METHOD_DEF].table)[index];
}

static inline metadata_field_t* metadata_get_field(metadata_t* metadata, int index) {
    ASSERT(index < metadata->tables[METADATA_FIELD].rows);
    return &((metadata_field_t*)metadata->tables[METADATA_FIELD].table)[index];
}

/**
 * Parse all the metadata stream into the metadata structure organized in nice addressable tables
 *
 * @param file      [IN]    The assembly the metadata is related to
 * @param stream    [IN]    The stream to parse
 * @param size      [IN]    The size of the stream to parse
 * @param metadata  [OUT]   The metadata output
 * @return
 */
err_t metadata_parse(pe_file_t* file, void* stream, size_t size, metadata_t* metadata);

/**
 * Free all the allocated metadata
 *
 * @param metadata  [IN]    The allocated metadata
 */
void free_metadata(metadata_t* metadata);

