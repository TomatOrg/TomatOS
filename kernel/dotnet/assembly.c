#include "assembly.h"
#include "types.h"

#include <dotnet/metadata/metadata_spec.h>
#include <dotnet/metadata/metadata.h>
#include <dotnet/parameter_info.h>
#include <dotnet/method_info.h>
#include <dotnet/field_info.h>
#include <dotnet/dotnet.h>
#include <dotnet/type.h>

#include <util/string.h>
#include <util/except.h>
#include <mem/mem.h>
#include <dotnet/pe/pe.h>
#include <dotnet/metadata/signature.h>

assembly_t g_corlib = NULL;

static err_t validate_and_set_assembly_name(assembly_t assembly, metadata_t* metadata) {
    err_t err = NO_ERROR;

    // the metadata table should have exactly one entry
    CHECK(metadata->tables[METADATA_MODULE].rows == 1);

    // get the module
    metadata_module_t* module = metadata->tables[METADATA_MODULE].table;
    CHECK(module->name != NULL && module->name[0] != '\0');
    CHECK(module->mvid != NULL);

    // set the output
    assembly->name = module->name;
    assembly->guid = *module->mvid;

    cleanup:
    return err;
}

typedef struct tiny_header {
    uint8_t flags : 2;
    uint8_t size : 6;
} PACKED tiny_header_t;

typedef struct fat_header {
    uint16_t flags : 12;
    uint16_t size : 4;
    uint16_t max_stack;
    uint16_t code_size;
    uint32_t local_var_sig_tok;
} PACKED fat_header_t;

static err_t initialize_method_body(method_info_t method, const uint8_t* il, size_t left) {
    err_t err = NO_ERROR;

    CHECK(left >= 1);

    tiny_header_t* tiny_header = (tiny_header_t*) il;
    if (tiny_header->flags == 0x2) {
        // tiny format
        il++;
        left--;

        method->il_size = tiny_header->size;
        method->max_stack_size = 8;
    } else if (tiny_header->flags == 0x3) {
        // get the format and verify it
        CHECK(left >= sizeof(fat_header_t));
        fat_header_t* fat_header = (fat_header_t*) il;
        CHECK(left >= fat_header->size * 4);
        il += fat_header->size * 4;
        left -= fat_header->size * 4;

        method->il_size = fat_header->code_size;
        method->max_stack_size = fat_header->max_stack;
    } else {
        CHECK_FAIL();
    }

    // copy the il code
    CHECK(left >= method->il_size);
    method->il = malloc(method->il_size);
    memcpy(method->il, il, method->il_size);

cleanup:
    return err;
}

static err_t initialize_types(assembly_t assembly, parsed_pe_t* parsed_pe, metadata_t* metadata) {
    err_t err = NO_ERROR;

    metadata_type_def_t* type_defs = metadata->tables[METADATA_TYPE_DEF].table;
    metadata_field_t* fields = metadata->tables[METADATA_FIELD].table;
    metadata_method_def_t* method_defs = metadata->tables[METADATA_METHOD_DEF].table;

    metadata_nested_class_t* nested_classes = metadata->tables[METADATA_NESTED_CLASS].table;
    size_t nested_class_rows = metadata->tables[METADATA_NESTED_CLASS].rows;
    for (int i = 0; i < nested_class_rows; i++) {
        type_t nested_class = assembly_get_type_by_token(assembly, nested_classes->nested_class);
        type_t enclosing_class = assembly_get_type_by_token(assembly, nested_classes->enclosing_class);
        nested_class->declaring_type = enclosing_class;
    }

    for (int i = 0; i < assembly->types_count; i++) {
        metadata_type_def_t* type_def = &type_defs[i];
        type_t type = &assembly->types[i];

        type->assembly = assembly;
        type->name = type_def->type_name;
        type->namespace = type_def->type_namespace;
        type->attributes = type_def->flags;
        type->base_type = assembly_get_type_by_token(assembly, type_def->extends);

        if (i + 1 == assembly->types_count) {
            // get the runlist from the last type
            type->methods_count = assembly->methods_count - type_def->method_list.index + 1;
        } else {
            // get the runlist from the next type
            type->methods_count = type_defs[i + 1].method_list.index - type_def->method_list.index;
        }

        if (type->methods_count > 0) {
            type->methods = assembly_get_method_info_by_token(assembly, type_def->method_list);
            CHECK(type->methods != NULL);

            for (int j = 0; j < type->methods_count; j++) {
                metadata_method_def_t* method_def = &method_defs[type_def->method_list.index - 1 + j];
                method_info_t method_info = &type->methods[j];
                method_info->assembly = assembly;
                method_info->name = method_def->name;
                method_info->attributes = method_def->flags;
                method_info->declaring_type = type;
                method_info->metadata_token = (token_t){
                        .table = METADATA_METHOD_DEF,
                        .index = type_def->method_list.index + j
                };

                // get the il code
                pe_directory_t directory = {
                        .rva = method_def->rva
                };
                const uint8_t* cil = pe_get_rva_ptr(parsed_pe, &directory);
                CHECK(cil != NULL);

                // parse the method body
                CHECK_AND_RETHROW(initialize_method_body(method_info, cil, directory.size));

                // parse the method signature
                CHECK_AND_RETHROW(sig_parse_method(method_def->signature, method_info));
            }
        }

        if (i + 1 == assembly->types_count) {
            // get the runlist from the last type
            type->fields_count = assembly->fields_count - type_def->field_list.index + 1;
        } else {
            // get the runlist from the next type
            type->fields_count = type_defs[i + 1].field_list.index - type_def->field_list.index;
        }

        if (type->fields_count > 0) {
            type->fields = assembly_get_field_info_by_token(assembly, type_def->field_list);
            CHECK(type->fields != NULL);

            for (int j = 0; j < type->fields_count; j++) {
                metadata_field_t* field = &fields[type_def->field_list.index - 1 + j];
                field_info_t field_info = &type->fields[j];
                field_info->assembly = assembly;
                field_info->name = field->name;
                field_info->attributes = field->flags;
                field_info->declaring_type = type;
                field_info->metadata_token = (token_t){
                        .table = METADATA_FIELD,
                        .index = type_def->field_list.index + j
                };

                // parse the type
                CHECK_AND_RETHROW(sig_parse_field(field->signature, assembly, field_info));
            }
        }
    }

    cleanup:
    return err;
}

static err_t initialize_type_size(assembly_t assembly, type_t type) {
    err_t err = NO_ERROR;
    int alignment = 0;
    int size = 0;

    // check if already initialized
    if (type->inited_size) goto cleanup;
    type->inited_size = true;

    // initialize the declaring type, if any
    if (type->declaring_type != NULL) {
        CHECK_AND_RETHROW(initialize_type_size(assembly, type->declaring_type));
        CHECK(type->managed_alignment != -1 && type->managed_size != -1);
        type->is_value_type = type->declaring_type->is_value_type;

        alignment = type->declaring_type->managed_alignment;
        size = type->declaring_type->managed_size;
    }

    if (type->is_value_type) {
        // initialize to -1 to detect recursive types
        type->stack_alignment = -1;
        type->stack_size = -1;
        type->managed_alignment = -1;
        type->managed_size = -1;
    } else {
        // reference types are like pointers on the stack
        type->stack_alignment = g_nuint->stack_alignment;
        type->stack_size = g_nuint->stack_size;
    }

    // calculate the size and alignment
    for (int j = 0; j < type->fields_count; j++) {
        field_info_t field_info = &type->fields[j];

        if (field_is_static(field_info)) {
            continue;
        }

        initialize_type_size(assembly, field_info->field_type);
        CHECK(field_info->field_type->stack_alignment >= 0 && field_info->field_type->stack_size >= 0);
        alignment = MAX(alignment, field_info->field_type->stack_alignment);
        size = ALIGN_UP(size, field_info->field_type->stack_alignment);
        size += field_info->field_type->stack_size;
    }

    // Align the size of the whole type to the alignment
    // of this object
    size = ALIGN_UP(size, alignment);

    // set the calculated alignment of the size
    type->managed_alignment = alignment;
    type->managed_size = size;

    // value types are the same on the stack as on the heap
    if (type->is_value_type) {
        type->stack_alignment = type->managed_alignment;
        type->stack_size = type->managed_size;
    }

    cleanup:
    return err;
}

static err_t initialize_type_sizes(assembly_t assembly) {
    err_t err = NO_ERROR;

    for (int i = 0; i < assembly->types_count; i++) {
        type_t type = &assembly->types[i];
        CHECK_AND_RETHROW(initialize_type_size(assembly, type));
    }

    cleanup:
    return err;
}

err_t load_assembly_from_blob(assembly_t* out_assembly, const void* buffer, size_t buffer_size) {
    err_t err = NO_ERROR;
    parsed_pe_t pe = { 0 };

    // allocate a new assembly
    assembly_t assembly = malloc(sizeof(struct assembly));
    CHECK_ERROR(assembly != NULL, ERROR_OUT_OF_RESOURCES);

    // parse the pe and the metadata from the pe
    pe = (parsed_pe_t){
            .assembly = assembly,
            .blob = buffer,
            .blob_size = buffer_size
    };
    CHECK_AND_RETHROW(pe_parse(&pe));

    // load everything from the loaded assembly
    CHECK_AND_RETHROW(validate_and_set_assembly_name(assembly, &pe.metadata));

    // allocate space for the types
    assembly->types_count = pe.metadata.tables[METADATA_TYPE_DEF].rows;
    assembly->types = malloc(assembly->types_count * sizeof(struct type));
    CHECK_ERROR(assembly->types != NULL, ERROR_OUT_OF_RESOURCES);

    assembly->methods_count = pe.metadata.tables[METADATA_METHOD_DEF].rows;
    assembly->methods = malloc(assembly->methods_count * sizeof(struct type));
    CHECK_ERROR(assembly->methods != NULL, ERROR_OUT_OF_RESOURCES);

    assembly->fields_count = pe.metadata.tables[METADATA_FIELD].rows;
    assembly->fields = malloc(assembly->fields_count * sizeof(struct type));
    CHECK_ERROR(assembly->fields != NULL, ERROR_OUT_OF_RESOURCES);

    // get the entry point if any
    assembly->entry_point = assembly_get_method_info_by_token(assembly, pe.cli_header->entry_point_token);

    // assume the first library loaded is the corlib
    if (g_corlib == NULL) {
        g_corlib = assembly;

        // setup all the base types before we continue and set the rest of
        // the type information
        CHECK_AND_RETHROW(initialize_base_types(
                pe.metadata.tables[METADATA_TYPE_DEF].table,
                pe.metadata.tables[METADATA_TYPE_DEF].rows));
    }

    // initialize the type information and then initialize the type sizes
    CHECK_AND_RETHROW(initialize_types(assembly, &pe, &pe.metadata));
    CHECK_AND_RETHROW(initialize_type_sizes(assembly));

    // set the output assembly
    *out_assembly = assembly;

    TRACE("Successfully loaded new assembly `%s`", assembly->name);

    cleanup:
    // we no longer need it
    free_parsed_pe(&pe);

    if (IS_ERROR(err)) {
        // TODO: free_assembly

        // free the assembly if there was an error
        free(assembly);
    }
    return err;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

type_t assembly_get_type_by_token(struct assembly* assembly, token_t token) {
    switch (token.table) {
        case METADATA_TYPE_DEF:
            if (token.index == 0 || token.index - 1 >= assembly->types_count) {
                return NULL;
            }
            return &assembly->types[token.index - 1];

        default:
            return NULL;
    }
}

method_info_t assembly_get_method_info_by_token(struct assembly* assembly, token_t token) {
    if (token.table != METADATA_METHOD_DEF) return NULL;
    if (token.index == 0 || token.index - 1 >= assembly->methods_count) return NULL;
    return &assembly->methods[token.index - 1];
}

field_info_t assembly_get_field_info_by_token(struct assembly* assembly, token_t token) {
    if (token.table != METADATA_FIELD) return NULL;
    if (token.index == 0 || token.index - 1 >= assembly->fields_count) return NULL;
    return &assembly->fields[token.index - 1];
}
