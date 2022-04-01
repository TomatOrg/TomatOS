#include "loader.h"

#include "types.h"
#include "runtime/dotnet/gc/gc.h"
#include "encoding.h"

#include <runtime/dotnet/metadata/metadata.h>

#include <util/string.h>
#include <mem/mem.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Decoding the binary
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static err_t decode_metadata(pe_file_t* ctx, metadata_t* metadata) {
    err_t err = NO_ERROR;
    void* metadata_root = NULL;

    // get the metadata
    metadata_root = pe_get_rva_data(ctx, ctx->cli_header->metadata);
    CHECK_ERROR(metadata_root != NULL, ERROR_NOT_FOUND);

    // parse it
    CHECK_AND_RETHROW(metadata_parse(ctx, metadata_root, ctx->cli_header->metadata.size, metadata));

cleanup:
    // we no longer need this
    free(metadata_root);
    return err;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Generic parsing
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static err_t init_type(metadata_t* metadata, System_Reflection_Assembly Assembly, System_Type type, int index) {
    err_t err = NO_ERROR;

    metadata_type_def_t* type_def = metadata_get_type_def(metadata, index);

    // set the assembly
    GC_UPDATE(type, Assembly, Assembly);

    GC_UPDATE(type, Name, new_string_from_utf8(type_def->type_name, strlen(type_def->type_name)));
    GC_UPDATE(type, Namespace, new_string_from_utf8(type_def->type_namespace, strlen(type_def->type_namespace)));

    // handle the extends
    if (type_def->extends.index != 0) {
        GC_UPDATE(type, BaseType, get_type_by_token(Assembly, type_def->extends));
    }

    // get the fields count
    size_t fields_base = type_def->field_list.index - 1;
    size_t fields_count = 0;
    CHECK(type_def->field_list.table == METADATA_FIELD);
    if (index + 1 == metadata->tables[METADATA_TYPE_DEF].rows) {
        fields_count = metadata->tables[METADATA_FIELD].rows - fields_base;
    } else {
        metadata_type_def_t* next_type_def = metadata_get_type_def(metadata, index + 1);
        CHECK(next_type_def->field_list.table == METADATA_FIELD);
        fields_count = (next_type_def->field_list.index - 1) - fields_base;
    }
    CHECK(fields_base + fields_count <= metadata->tables[METADATA_FIELD].rows);

    // Allocate the array of fields
    GC_UPDATE(type, Fields, GC_NEW_ARRAY(tSystem_Reflection_FieldInfo, fields_count));
    for (size_t i = 0; i < fields_count; i++) {
        metadata_field_t* field_def = metadata_get_field(metadata, fields_base + i);
        System_Reflection_FieldInfo fieldInfo = GC_NEW(tSystem_Reflection_FieldInfo);

        // init the field
        fieldInfo->Attributes = field_def->flags;
        GC_UPDATE(fieldInfo, Name, new_string_from_utf8(field_def->name, strlen(field_def->name)));

        // store it
        GC_UPDATE_ARRAY(type->Fields, i, fieldInfo);
    }

cleanup:
    return err;
}

static err_t init_module(metadata_t* metadata, System_Reflection_Assembly Assembly) {
    err_t err = NO_ERROR;

    CHECK(metadata->tables[METADATA_MODULE].rows == 1, "The Module table shall contain one and only one row");
    metadata_module_t* md_module = metadata->tables[METADATA_MODULE].table;

    System_Reflection_Module Module = GC_NEW(tSystem_Reflection_Module);
    GC_UPDATE(Module, Assembly, Assembly);
    GC_UPDATE(Module, Name, new_string_from_utf8(md_module->name, strlen(md_module->name)));

cleanup:
    return err;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Parse the corelib
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct builtin_type {
    const char* namespace;
    const char* name;
    System_Type* global;
    size_t stack_size;
    size_t managed_size;
} builtin_type_t;

#define BUILTIN_TYPE(_namespace, _name, code) \
    { .namespace = (_namespace), .name = (_name), &t##code, sizeof(code), sizeof(struct code) }

static builtin_type_t m_builtin_types[] = {
    BUILTIN_TYPE("System", "Type", System_Type),
    BUILTIN_TYPE("System", "Array", System_Array),
    BUILTIN_TYPE("System", "String", System_String),
    BUILTIN_TYPE("System.Reflection", "Module", System_Reflection_Module),
    BUILTIN_TYPE("System.Reflection", "Assembly", System_Reflection_Assembly),
    BUILTIN_TYPE("System.Reflection", "FieldInfo", System_Reflection_FieldInfo),
};

static void init_builtin_type(metadata_type_def_t* type_def, System_Type type) {
    // check if this is a builtin type
    for (int i = 0; i < ARRAY_LEN(m_builtin_types); i++) {
        builtin_type_t* bt = &m_builtin_types[i];
        if (
            strcmp(type_def->type_namespace, bt->namespace) == 0 &&
            strcmp(type_def->type_name, bt->name) == 0
        ) {
            // TODO: verify the type in the assembly has the same size as we expect from
            //       the c struct representation
            type->managed_size = bt->managed_size;
            type->stack_size = bt->stack_size;
            *bt->global = type;
            break;
        }
    }
}

err_t loader_load_corelib(void* buffer, size_t buffer_size) {
    err_t err = NO_ERROR;

    TRACE("Initializing corelib");

    // Start by loading the PE file for the corelib
    pe_file_t file = {
        .file = buffer,
        .file_size = buffer_size
    };
    CHECK_AND_RETHROW(pe_parse(&file));

    // decode the dotnet metadata
    metadata_t metadata = { 0 };
    CHECK_AND_RETHROW(decode_metadata(&file, &metadata));

    // We have not loaded any type yet, use null for now until we boostrap
    // the typing system
    System_Reflection_Assembly assembly = gc_new(NULL, sizeof(struct System_Reflection_Assembly));

    // allocate the defined types
    int type_count = metadata.tables[METADATA_TYPE_DEF].rows;
    assembly->DefinedTypes = gc_new(NULL, sizeof(struct System_Array) + type_count * sizeof(struct System_Type));
    assembly->DefinedTypes->Length = type_count;

    // fill in with empty objects for now, that is needed so we can at least have
    // the type itself even if it will be zeroed out
    for (int i = 0; i < type_count; i++) {
        metadata_type_def_t* type_def = metadata_get_type_def(&metadata, i);
        System_Type type = gc_new(NULL, sizeof(struct System_Type));
        assembly->DefinedTypes->Data[i] = type;
        init_builtin_type(type_def, type);
    }

    // validate we got all the builtin types
    for (int i = 0; i < ARRAY_LEN(m_builtin_types); i++) {
        builtin_type_t* bt = &m_builtin_types[i];
        CHECK(*bt->global != NULL, "Failed to find builtin type `%s.%s`", bt->namespace, bt->name);
    }

    // now we have all the types, set the type of the array and the type of the types
    assembly->type = tSystem_Reflection_Assembly;
    assembly->DefinedTypes->type = get_array_type(tSystem_Type);
    for (int i = 0; i < type_count; i++) {
        assembly->DefinedTypes->Data[i]->type = tSystem_Type;
    }

    //
    // now we can continue with loading the rest normally
    //

    // initialize the module itself
    CHECK_AND_RETHROW(init_module(&metadata, assembly));

    for (int i = 0; i < type_count; i++) {
        CHECK_AND_RETHROW(init_type(&metadata, assembly, assembly->DefinedTypes->Data[i], i));
    }

    TRACE("Types:");
    for (int i = 0; i < assembly->DefinedTypes->Length; i++) {
        System_Type type = assembly->DefinedTypes->Data[i];
        TRACE("\t%U.%U", type->Namespace, type->Name);

        for (int j = 0; j < type->Fields->Length; j++) {
            CHECK(type->Fields->Data[j] != NULL);
            TRACE("\t\t%U", type->Fields->Data[j]->Name);
        }
    }

cleanup:
    return err;
}
