#include "metadata.h"

#include "metadata_spec.h"
#include "sig.h"

#include <util/string.h>
#include <util/defs.h>
#include <mem/mem.h>

#include <stdint.h>
#include <stddef.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Binary structs only relevant internally
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct cli_metadata_root {
    uint32_t signature;
#define CLI_METADATA_ROOT_SIGNATURE 0x424A5342
    uint16_t major_version;
    uint16_t minor_version;
    uint32_t _reserved;
    uint32_t length;
    char version[0];
} PACKED cli_metadata_root_t;
STATIC_ASSERT(sizeof(cli_metadata_root_t) == 16);

#define CLI_METADATA_ROOT_STREAMS(root) \
    ({ \
        cli_metadata_root_t* __root = (cli_metadata_root_t*)(root); \
        *((uint16_t*)(((uintptr_t)__root) + sizeof(cli_metadata_root_t) + __root->length + 2)); \
    })

#define CLI_METADATA_ROOT_STREAM_HEADERS(root) \
    ({ \
        cli_metadata_root_t* __root = (cli_metadata_root_t*)(root); \
        (cli_stream_header_t*)(((uintptr_t)__root) + sizeof(cli_metadata_root_t) + __root->length + 4); \
    })

typedef struct cli_stream_header {
    uint32_t offset;
    uint32_t size;
    char name[];
} PACKED cli_stream_header_t;

typedef struct cli_metadata_stream {
    uint32_t _reserved;
    uint8_t major_version;
    uint8_t minor_version;
    uint8_t heap_sizes;
    uint8_t _reserved1;
    uint64_t valid;
    uint64_t sorted;
    uint32_t rows[];
} PACKED cli_metadata_stream_t;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Structs to help with parsing the tables
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * The type of coded indexes
 */
typedef enum coded_index {
    TYPE_DEF_OR_REF,
    HAS_CONSTANT,
    HAS_CUSTOM_ATTRIBUTE,
    HAS_FIELD_MARSHALL,
    HAS_DECL_SECURITY,
    MEMBER_REF_PARENT,
    HAS_SEMANTICS,
    METHOD_DEF_OR_REF,
    MEMBER_FORWARDED,
    IMPLEMENTATION,
    CUSTOM_ATTRIBUTE_TYPE,
    RESOLUTION_SCOPE,
    TYPE_OR_METHOD_DEF,
    CODED_INDEX_MAX,
} coded_index_t;

typedef struct metadata_parse_ctx {
    // the tables
    bool long_string_index;
    bool long_guid_index;
    bool long_blob_index;

    // the amount of items in each of the tables
    bool long_coded_index[CODED_INDEX_MAX];

    // the metadata
    metadata_t* metadata;

    // the size of the tables stream
    uint8_t* table;
    size_t size;

    // the assembly we are parsing for
    pe_file_t* file;
} metadata_parse_ctx_t;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Parsing helpers
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * The amount of bits for each of the coded indexes
 */
static uint8_t m_coded_index_bits[] = {
    [TYPE_DEF_OR_REF] = 2,
    [HAS_CONSTANT] = 2,
    [HAS_CUSTOM_ATTRIBUTE] = 5,
    [HAS_FIELD_MARSHALL] = 1,
    [HAS_DECL_SECURITY] = 2,
    [MEMBER_REF_PARENT] = 3,
    [HAS_SEMANTICS] = 1,
    [METHOD_DEF_OR_REF] = 1,
    [MEMBER_FORWARDED] = 1,
    [IMPLEMENTATION] = 2,
    [CUSTOM_ATTRIBUTE_TYPE] = 3,
    [RESOLUTION_SCOPE] = 2,
    [TYPE_OR_METHOD_DEF] = 1,
};

/**
 * A helper for decoding coded indexes, contains the actual table indexes
 */
static const char* m_coded_index_tags[] = {
    [TYPE_DEF_OR_REF] = "\x02\x01\x1Bz",
    [HAS_CONSTANT] = "\x04\x08\x17z",
    [HAS_CUSTOM_ATTRIBUTE] = "\x06\x04\x01\x02\x08\x09\x0A\x00\x0E\x17\x14\x11\x1A\x1B\x20\x23\x26\x27\x28zzzzzzzzzzzzz",
    [HAS_FIELD_MARSHALL] = "\x04\x08",
    [HAS_DECL_SECURITY] = "\x02\x06\x20z",
    [MEMBER_REF_PARENT] = "\x02\x01\x1A\x06\x1Bzzz",
    [HAS_SEMANTICS] = "\x14\x17",
    [METHOD_DEF_OR_REF] = "\x06\x0A",
    [MEMBER_FORWARDED] = "\x04\x06",
    [IMPLEMENTATION] = "\x26\x23\x27z",
    [CUSTOM_ATTRIBUTE_TYPE] = "zz\x06\x0Azzzz",
    [RESOLUTION_SCOPE] = "\x00\x1A\x23\x01",
    [TYPE_OR_METHOD_DEF] = "\x02\x06",
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Table parsing
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef enum table_parse_op {
    // parsing stream is done
    DONE,

    // Get integers
    GET_UINT16,
    GET_UINT32,
    GET_RVA,

    // Get heap values
    GET_BLOB,
    GET_GUID,
    GET_STRING,

    // Coded index fetching
    GET_CODED_INDEX_BASE,
    GET_TYPE_DEF_OR_REF = GET_CODED_INDEX_BASE,
    GET_HAS_CONSTANT,
    GET_HAS_CUSTOM_ATTRIBUTE,
    GET_HAS_FIELD_MARSHALL,
    GET_HAS_DECL_SECURITY,
    GET_MEMBER_REF_PARENT,
    GET_HAS_SEMANTICS,
    GET_METHOD_DEF_OR_REF,
    GET_MEMBER_FORWARDED,
    GET_IMPLEMENTATION,
    GET_CUSTOM_ATTRIBUTE_TYPE,
    GET_RESOLUTION_SCOPE,
    GET_TYPE_OR_METHOD_DEF,
    GET_CODED_INDEX_MAX = GET_TYPE_OR_METHOD_DEF,

    GET_TABLE_BASE,
    GET_TYPE_DEF = GET_TABLE_BASE + METADATA_TYPE_DEF,
    GET_FIELD = GET_TABLE_BASE + METADATA_FIELD,
    GET_METHOD_DEF = GET_TABLE_BASE + METADATA_METHOD_DEF,
    GET_PARAM = GET_TABLE_BASE + METADATA_PARAM,
    GET_EVENT = GET_TABLE_BASE + METADATA_EVENT,
    GET_PROPERTY = GET_TABLE_BASE + METADATA_PROPERTY,
    GET_ASSEMBLY_REF = GET_TABLE_BASE + METADATA_ASSEMBLY_REF,
    GET_GENERIC_PARAM = GET_TABLE_BASE + METADATA_GENERIC_PARAM,
    GET_TABLE_MAX = GET_GENERIC_PARAM,
} table_parse_op_t;

/**
 * These are the fields that we have inside each of the table rows, this will
 * be used to parse them into nicely usable structs
 */
static table_parse_op_t* m_table_ops[] = {
    [METADATA_MODULE] = (table_parse_op_t[]){ GET_UINT16, GET_STRING, GET_GUID, GET_GUID, GET_GUID, DONE },
    [METADATA_TYPE_REF] = (table_parse_op_t[]){ GET_RESOLUTION_SCOPE, GET_STRING, GET_STRING, DONE },
    [METADATA_TYPE_DEF] = (table_parse_op_t[]){ GET_UINT32, GET_STRING, GET_STRING, GET_TYPE_DEF_OR_REF, GET_FIELD, GET_METHOD_DEF, DONE },
    [METADATA_FIELD] = (table_parse_op_t[]){ GET_UINT16, GET_STRING, GET_BLOB, DONE },
    [METADATA_METHOD_DEF] = (table_parse_op_t[]){ GET_RVA, GET_UINT16, GET_UINT16, GET_STRING, GET_BLOB, GET_PARAM, DONE },
    [METADATA_PARAM] = (table_parse_op_t[]){ GET_UINT16, GET_UINT16, GET_STRING, DONE },
    [METADATA_INTERFACE_IMPL] = (table_parse_op_t[]){ GET_TYPE_DEF, GET_TYPE_DEF_OR_REF, DONE },
    [METADATA_MEMBER_REF] = (table_parse_op_t[]){ GET_MEMBER_REF_PARENT, GET_STRING, GET_BLOB, DONE },
    [METADATA_CONSTANT] = (table_parse_op_t[]) { GET_UINT16, GET_HAS_CONSTANT, GET_BLOB, DONE },
    [METADATA_CUSTOM_ATTRIBUTE] = (table_parse_op_t[]){ GET_HAS_CUSTOM_ATTRIBUTE, GET_CUSTOM_ATTRIBUTE_TYPE, GET_BLOB, DONE },
    [METADATA_DECL_SECURITY] = (table_parse_op_t[]){ GET_UINT16, GET_HAS_DECL_SECURITY, GET_BLOB, DONE },
    [METADATA_CLASS_LAYOUT] = (table_parse_op_t[]){ GET_UINT16, GET_UINT32, GET_TYPE_DEF, DONE },
    [METADATA_FIELD_LAYOUT] = (table_parse_op_t[]){ GET_UINT32, GET_FIELD, DONE },
    [METADATA_STAND_ALONE_SIG] = (table_parse_op_t[]){ GET_BLOB, DONE },
    [METADATA_EVENT_MAP] = (table_parse_op_t[]){ GET_TYPE_DEF, GET_EVENT, DONE },
    [METADATA_EVENT] = (table_parse_op_t[]){ GET_UINT16, GET_STRING, GET_TYPE_DEF_OR_REF, DONE },
    [METADATA_PROPERTY_MAP] = (table_parse_op_t[]){ GET_TYPE_DEF, GET_PROPERTY, DONE },
    [METADATA_PROPERTY] = (table_parse_op_t[]){ GET_UINT16, GET_STRING, GET_BLOB, DONE },
    [METADATA_METHOD_SEMANTICS] = (table_parse_op_t[]){ GET_UINT16, GET_METHOD_DEF, GET_HAS_SEMANTICS, DONE },
    [METADATA_METHOD_IMPL] = (table_parse_op_t[]){ GET_TYPE_DEF, GET_METHOD_DEF_OR_REF, GET_METHOD_DEF_OR_REF, DONE },
    [METADATA_TYPE_SPEC] = (table_parse_op_t[]){ GET_BLOB, DONE },
    [METADATA_ASSEMBLY] = (table_parse_op_t[]){ GET_UINT32, GET_UINT16, GET_UINT16, GET_UINT16, GET_UINT16, GET_UINT32, GET_BLOB, GET_STRING, GET_STRING, DONE },
    [METADATA_ASSEMBLY_REF] = (table_parse_op_t[]){ GET_UINT16, GET_UINT16, GET_UINT16, GET_UINT16, GET_UINT32, GET_BLOB, GET_STRING, GET_STRING, GET_BLOB, DONE },
    [METADATA_ASSEMBLY_REF_OS] = (table_parse_op_t[]){ GET_UINT32, GET_UINT32, GET_UINT32, GET_ASSEMBLY_REF, DONE },
    [METADATA_EXPORTED_TYPE] = (table_parse_op_t[]){ GET_UINT32, GET_UINT32, GET_STRING, GET_STRING, GET_IMPLEMENTATION, DONE },
    [METADATA_NESTED_CLASS] = (table_parse_op_t[]){ GET_TYPE_DEF, GET_TYPE_DEF, DONE },
    [METADATA_GENERIC_PARAM] = (table_parse_op_t[]){ GET_UINT16, GET_UINT16, GET_TYPE_OR_METHOD_DEF, GET_STRING, DONE },
    [METADATA_GENERIC_PARAM_CONSTRAINT] = (table_parse_op_t[]){ GET_GENERIC_PARAM, GET_TYPE_DEF_OR_REF, DONE },
};

/**
 * Get a uint16 from the table
 */
#define FETCH_UINT16() \
    ({ \
        uint16_t __value = *(uint16_t*)ctx->table; \
        ctx->table += 2; \
        __value; \
    })

/**
 * Get a uint32 from the table
 */
#define FETCH_UINT32() \
    ({ \
        uint32_t __value = *(uint32_t*)ctx->table; \
        ctx->table += 4; \
        __value; \
    })

static err_t parse_single_table(metadata_parse_ctx_t* ctx, int table_id) {
    err_t err = NO_ERROR;

    // get the correct thing
    CHECK(table_id < ARRAY_LEN(m_table_ops), "unknown table id %x", table_id);
    table_parse_op_t* ops = m_table_ops[table_id];
    CHECK(ops != NULL, "unknown table id %x", table_id);

    // get the amount of rows
    size_t rows = ctx->metadata->tables[table_id].rows;

    // figure the row size for allocation
    size_t row_size = 0;
    size_t in_memory_size = 0;
    table_parse_op_t* cur_op = ops;
    while (*cur_op != DONE) {
        switch (*cur_op) {
            case GET_RVA: row_size += 4; in_memory_size += 4; break;
            case GET_UINT16: row_size += 2; in_memory_size += 2; break;
            case GET_UINT32: row_size += 4; in_memory_size += 4; break;
            case GET_BLOB: row_size += ctx->long_blob_index ? 4 : 2; in_memory_size += sizeof(blob_entry_t); break;
            case GET_GUID: row_size += ctx->long_guid_index ? 4 : 2; in_memory_size += sizeof(void*); break;
            case GET_STRING: row_size += ctx->long_string_index ? 4 : 2; in_memory_size += sizeof(void*); break;
            case GET_CODED_INDEX_BASE ... GET_CODED_INDEX_MAX: row_size += ctx->long_coded_index[*cur_op - GET_CODED_INDEX_BASE] ? 4 : 2; in_memory_size += sizeof(token_t); break;
            case GET_TABLE_BASE ... GET_TABLE_MAX: row_size += ctx->metadata->tables[*cur_op - GET_TABLE_BASE].rows > UINT16_MAX ? 4 : 2; in_memory_size += sizeof(token_t); break;
            case DONE: break;
            default: CHECK_FAIL("Invalid opcode");
        }
        cur_op++;
    }

    // make sure the table is big enough
    CHECK(row_size * rows <= ctx->size);

    // allocate the table itself
    uint8_t* table = malloc(in_memory_size * rows);
    CHECK_ERROR(table != NULL, ERROR_OUT_OF_RESOURCES);
    ctx->metadata->tables[table_id].table = table;

    // now parse it
    uint8_t* table_before = ctx->table;
    for (int i = 0; i < rows; i++) {
        cur_op = ops;
        while (*cur_op != DONE) {
            switch (*cur_op) {
                case GET_UINT16: {
                    *(uint16_t*)table = FETCH_UINT16();
                    table += 2;
                }  break;

                case GET_UINT32: {
                    *(uint32_t*)table = FETCH_UINT32();
                    table += 4;
                } break;

                case GET_RVA: {
                    *(uint32_t*)table = FETCH_UINT32();
                    table += 4;
                } break;

                case GET_BLOB: {
                    // get the entry
                    uint32_t idx = ctx->long_blob_index ? FETCH_UINT32() : FETCH_UINT16();
                    CHECK(idx < ctx->file->blob_size);

                    // parse the compressed length
                    uint32_t blob_size = 0;
                    blob_entry_t entry = {
                        .data = &ctx->file->blob[idx],
                        .size = ctx->file->blob_size - idx,
                    };
                    CHECK_AND_RETHROW(parse_compressed_integer(&entry, &blob_size));

                    // validate and set the length
                    CHECK(blob_size <= ctx->file->blob_size - idx);
                    entry.size = blob_size;

                    // set the entry
                    *(blob_entry_t*)table = entry;
                    table += sizeof(blob_entry_t);
                } break;

                case GET_GUID: {
                    uint32_t idx = ctx->long_guid_index ? FETCH_UINT32() : FETCH_UINT16();
                    CHECK(idx == 0 || idx - 1 < ctx->file->guids_count);
                    *(guid_t**)table = idx == 0 ? NULL : &ctx->file->guids[idx - 1];
                    table += sizeof(guid_t*);
                } break;

                case GET_STRING: {
                    uint32_t idx = ctx->long_string_index ? FETCH_UINT32() : FETCH_UINT16();
                    CHECK(idx < ctx->file->strings_size);
                    *(const char**)table = &ctx->file->strings[idx];
                    table += sizeof(const char*);
                } break;

                case GET_CODED_INDEX_BASE ... GET_CODED_INDEX_MAX: {
                    // TODO: same with the table but in here we would need some kind
                    //       of a tagged union, which would work as well...
                    int offset = *cur_op - GET_CODED_INDEX_BASE;
                    CHECK(1 <= ctx->size);
                    uint8_t tag_bits = m_coded_index_bits[offset];
                    uint8_t tag = *ctx->table & ((1 << tag_bits) - 1);
                    uint8_t cur_table_id = m_coded_index_tags[offset][tag];
                    CHECK(cur_table_id < ARRAY_LEN(ctx->metadata->tables));
                    uint32_t table_index = ctx->long_coded_index[offset] ? FETCH_UINT32() : FETCH_UINT16();
                    table_index >>= tag_bits;
                    CHECK(table_index == 0 || table_index - 1 <= ctx->metadata->tables[cur_table_id].rows);
                    *(token_t*)table = (token_t){ .table = cur_table_id, .index = table_index };
                    table += sizeof(token_t);
                } break;

                case GET_TABLE_BASE ... GET_TABLE_MAX: {
                    // TODO: in theory if we first get the row sizes and only then start
                    //       parsing all the tables we can have this return the pointer
                    //       directly...
                    int cur_table_id = *cur_op - GET_TABLE_BASE;
                    uint32_t table_index = ctx->metadata->tables[cur_table_id].rows > UINT16_MAX ? FETCH_UINT32() : FETCH_UINT16();
                    CHECK(table_index == 0 || table_index - 1 <= ctx->metadata->tables[cur_table_id].rows, "%d %d", cur_table_id, table_index);
                    *(token_t*)table = (token_t){ .table = cur_table_id, .index = table_index };
                    table += sizeof(token_t);
                } break;

                case DONE: break;
                default: CHECK_FAIL("Invalid opcode");
            }
            cur_op++;
        }
    }

    // remove the size of it
    ctx->size -= rows * row_size;

cleanup:
    if (IS_ERROR(err)) {
        (void)0;
    }
    return err;
}

static void resolve_coded_index_sizes(metadata_parse_ctx_t* ctx) {
    for (int i = 0; i < ARRAY_LEN(ctx->long_coded_index); i++) {
        const char* coding = m_coded_index_tags[i];
        int tag_bits = m_coded_index_bits[i];

        // find the max amount of table entries
        uint32_t max_table_len = 0;
        for (int j = 0; j < (1 << tag_bits); j++) {
            char tag = coding[j];
            if (tag != 'z') {
                if (ctx->metadata->tables[tag].rows > max_table_len) {
                    max_table_len = ctx->metadata->tables[tag].rows;
                }
            }
        }

        // if it is larger than 16bit we use 32bit indexes
        ctx->long_coded_index[i] = max_table_len < (1 << (16 - tag_bits)) ? false : true;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Metadata parsing
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

err_t metadata_parse(pe_file_t* file, void* stream, size_t size, metadata_t* metadata) {
    err_t err = NO_ERROR;
    cli_metadata_root_t* metadata_root = stream;

    CHECK(sizeof(cli_metadata_root_t) <= size);
    CHECK(metadata_root->signature == CLI_METADATA_ROOT_SIGNATURE);
    CHECK(sizeof(cli_metadata_root_t) + metadata_root->length + 4);

    // get the streams
    uint16_t streams = CLI_METADATA_ROOT_STREAMS(metadata_root);
    cli_stream_header_t* stream_header = CLI_METADATA_ROOT_STREAM_HEADERS(metadata_root);

    // get all the streams we want
    cli_metadata_stream_t* metadata_stream = NULL;
    size_t metadata_stream_size = 0;
    for (int i = 0; i < streams; i++) {
        // verify the entry
        CHECK(stream_header->offset + stream_header->size <= size);
        void* data = (void*)metadata_root + stream_header->offset;

        // set the target in the assembly
        if (strcmp("#~", stream_header->name) == 0) {
            CHECK(sizeof(cli_metadata_stream_t) < stream_header->size);
            metadata_stream = data;
            metadata_stream_size = stream_header->size;
        } else {
            // allocate the target
            void* target = malloc(stream_header->size);
            CHECK_ERROR(target != NULL, ERROR_OUT_OF_RESOURCES);

            // figure which stream we wanna set
            if (strcmp("#Strings", stream_header->name) == 0) {
                file->strings = target;
                file->strings_size = stream_header->size;
            } else if (strcmp("#US", stream_header->name) == 0) {
                file->us = target;
                file->us_size = stream_header->size;
            } else if (strcmp("#GUID", stream_header->name) == 0) {
                file->guids = target;
                file->guids_count = stream_header->size / 16;
            } else if (strcmp("#Blob", stream_header->name) == 0) {
                file->blob = target;
                file->blob_size = stream_header->size;
            } else {
                // make sure to free the target before we fail
                free(target);
                CHECK_FAIL_ERROR(ERROR_BAD_FORMAT, "%s", stream_header->name);
            }

            // copy the data
            memcpy(target, data, stream_header->size);
        }

        // get the next entry
        stream_header = (void*)stream_header + ALIGN_UP(sizeof(cli_stream_header_t) + strlen(stream_header->name) + 1, 4);
    }
    CHECK(metadata_stream != NULL);

    // now we can parse metadata
    CHECK(metadata_stream->major_version == 2);
    CHECK(metadata_stream->minor_version == 0);

    // remove the size of the header
    metadata_stream_size -= sizeof(cli_metadata_stream_t);

    // parse all the metadata tables
    metadata_parse_ctx_t ctx = {
        .metadata = metadata,
        .long_string_index = metadata_stream->heap_sizes & 0x1 ? true : false,
        .long_guid_index = metadata_stream->heap_sizes & 0x2 ? true : false,
        .long_blob_index = metadata_stream->heap_sizes & 0x4 ? true : false,
        .file = file,
    };

    // set the rows
    uint32_t* rows = (uint32_t*)metadata_stream->rows;
    for (int i = 0; i < ARRAY_LEN(ctx.metadata->tables); i++) {
        if (metadata_stream->valid & (1ull << i)) {
            // check we have enough bytes
            CHECK(4 <= metadata_stream_size);

            // take the next 4-byte integer for the row size
            ctx.metadata->tables[i].rows = *rows;
            rows++;
            metadata_stream_size -= 4;
        }
    }

    // resolve the coded index sizes
    resolve_coded_index_sizes(&ctx);

    // we can set the table and left size for parsing
    ctx.table = (uint8_t*)rows;
    ctx.size = metadata_stream_size;

    for (int i = 0; i < ARRAY_LEN(ctx.metadata->tables); i++) {
        if (metadata_stream->valid & (1ull << i)) {
            CHECK_AND_RETHROW(parse_single_table(&ctx, i));
        }
    }

cleanup:
    return err;
}

void free_metadata(metadata_t* metadata) {
    for (int i = 0; i < ARRAY_LEN(metadata->tables); i++) {
        free(metadata->tables[i].table);
    }
}
