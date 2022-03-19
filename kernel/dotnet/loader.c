#include "loader.h"

#include "metadata/metadata.h"
#include "metadata/pe.h"

#include "gc.h"

#include <util/string.h>
#include <mem/mem.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// All the base types that we need
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

System_Type* typeof_System_Reflection_Assembly = NULL;
System_Type* typeof_System_Reflection_Module = NULL;
System_Type* typeof_System_Reflection_FieldInfo = NULL;

System_Type* typeof_System_Array = NULL;
System_Type* typeof_System_Type = NULL;
System_Type* typeof_System_String = NULL;

#define TYPEOF_INIT(_namespace, _name) \
    (typeof_init_entry_t) { \
        .name = #_name, \
        .namespace = #_namespace, \
        .store = &typeof_##_namespace##_##_name \
    }

typedef struct typeof_init_entry {
    const char* name;
    const char* namespace;
    System_Type** store;
} typeof_init_entry_t;

static typeof_init_entry_t m_typeof_init[] = {
    TYPEOF_INIT(System_Reflection, Assembly),
    TYPEOF_INIT(System_Reflection, Module),
    TYPEOF_INIT(System_Reflection, FieldInfo),

    TYPEOF_INIT(System, Array),
    TYPEOF_INIT(System, Type),
    TYPEOF_INIT(System, String),
};

//----------------------------------------------------------------------------------------------------------------------
// The assemblies that we need
//----------------------------------------------------------------------------------------------------------------------

/**
 * The core library
 */
System_Reflection_Assembly* g_corelib = NULL;

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
// Actually loading everything properly
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static bool compare_namespace(const char* target, const char* current) {
    while (*target != '\0' && *current != '\0') {
        if (*target != *current) {
            // special case, we store the name with `_` rather than `.` for simplicity
            if (*target == '_' && *current == '.') {
                target++;
                current++;
                continue;
            }

            // not the same
            return false;
        }

        target++;
        current++;
    }
    return *target == *current;
}

/**
 * Boostrap the library loading by allocating all the objects we need for basic handling
 */
static err_t boostrap_lib(metadata_type_def_t* type_defs, System_Reflection_Assembly* assembly) {
    err_t err = NO_ERROR;
    System_Type** types = SYSTEM_ARRAY(System_Type*, assembly->DefinedTypes);

    for (int i = 0; i < assembly->DefinedTypes->Length; i++) {
        metadata_type_def_t* type_def = &type_defs[i];
        types[i] = GC_NEW(System_Type);

        if (g_corelib == NULL) {
            // we are still loading the corelib, initialize all the basic types that
            // the runtime need to use by itself
            for (int j = 0; j < ARRAY_LEN(m_typeof_init); j++) {
                if (
                    compare_namespace(m_typeof_init[j].namespace, type_def->type_namespace) &&
                    strcmp(m_typeof_init[j].name, type_def->type_name) == 0
                ) {
                    CHECK(*m_typeof_init[j].store == NULL);
                    *m_typeof_init[j].store = types[0];
                }
            }
        }
    }

    if (g_corelib == NULL) {
        // make sure we got all the builtin types
        for (int j = 0; j < ARRAY_LEN(m_typeof_init); j++) {
            CHECK(*m_typeof_init[j].store != NULL,
                  "Missing type: `%s.%s`", m_typeof_init[j].namespace, m_typeof_init[j].name);
        }

        // set all the types of the types
        for (int i = 0; i < assembly->DefinedTypes->Length; i++) {
            types[i]->Object.Type = typeof_System_Type;
        }

        // set the type of the assembly
        assembly->Object.Type = typeof_System_Reflection_Assembly;
    }

cleanup:
    return err;
}

err_t load_assembly_from_memory(System_Reflection_Assembly** out_assembly, void* file, size_t file_size) {
    err_t err = NO_ERROR;
    metadata_t metadata = { 0 };

    // parse the pe of the corelib
    pe_file_t pe_file = {
        .file = file,
        .file_size = file_size
    };
    CHECK_AND_RETHROW(pe_parse(&pe_file));
    CHECK_AND_RETHROW(decode_metadata(&pe_file, &metadata));

    // Allocate the new assembly
    System_Reflection_Assembly* assembly = GC_NEW(System_Reflection_Assembly);

    // Get the type defs
    int type_count = metadata.tables[METADATA_TYPE_DEF].rows;
    metadata_type_def_t* type_defs = metadata.tables[METADATA_TYPE_DEF].table;

    int field_count = metadata.tables[METADATA_FIELD].rows;
    metadata_field_t* fields = metadata.tables[METADATA_FIELD].table;

    // get the proper array type, keep NULL if not initialized yet
    System_Type* typeArray = NULL;
    if (typeof_System_Array != NULL) {
        typeArray = get_array_type(typeof_System_Type);
    }

    // Allocate the type array
    assembly->DefinedTypes = GC_NEW_REF_ARRAY(System_Type, type_count);
    assembly->DefinedTypes->Length = type_count;
    System_Type** types = SYSTEM_ARRAY(System_Type*, assembly->DefinedTypes);

    // check for some basic types first
    CHECK_AND_RETHROW(boostrap_lib(type_defs, assembly));

    // set the type array type if was null before
    if (typeArray == NULL) {
        typeArray = get_array_type(typeof_System_Type);
        assembly->DefinedTypes->Object.Type = typeArray;
    }

    if (g_corelib == NULL) {
        // save the assembly
        g_corelib = assembly;
    }

    // create the module
    CHECK(metadata.tables[METADATA_MODULE].rows == 1);
    metadata_module_t* metadata_module = metadata.tables[METADATA_MODULE].table;
    System_Reflection_Module* module = GC_NEW(System_Reflection_Module);
    module->Name = new_string_from_cstr(metadata_module->name);
    module->Assembly = assembly;

    // Initialize all the types in the array
    for (int i = 0; i < type_count; i++) {
        metadata_type_def_t* type_def = &type_defs[i];
        System_Type* current_type = types[i];

        // set the assembly
        current_type->Assembly = assembly;

        // create the names
        current_type->Name = new_string_from_cstr(type_def->type_name);
        current_type->Namespace = new_string_from_cstr(type_def->type_namespace);

        // get the last index of the methods and fields
        bool is_last = i == type_count - 1;
        int num_methods = (is_last ? metadata.tables[METADATA_METHOD_DEF].rows : type_def->method_list.index) - type_def->method_list.index + 1;
        int num_fields = (is_last ? field_count : type_def->field_list.index) - type_def->field_list.index + 1;

        // init fields
        current_type->Fields = GC_NEW_REF_ARRAY(System_Reflection_FieldInfo, num_fields);
        for (int j = 0; j < num_fields; j++) {
            metadata_field_t* field = &fields[j + type_def->field_list.index - 1];
            System_Reflection_FieldInfo* field_info = GC_NEW(System_Reflection_FieldInfo);
            SYSTEM_ARRAY(System_Reflection_FieldInfo*, current_type->Fields)[j] = field_info;

            // set the member info
            field_info->MemberInfo.Name = new_string_from_cstr(field->name);
            field_info->MemberInfo.DeclaringType = current_type;
            field_info->MemberInfo.Module = module;

            // set the field info
            field_info->Attributes = field->flags;

            // TODO: parse signature
        }

        // TODO: init methods
    }

cleanup:
    if (!IS_ERROR(err)) {
        if (out_assembly != NULL) {
            *out_assembly = assembly;
        }
    }

    free_pe_file(&pe_file);
    free_metadata(&metadata);
    return err;
}

