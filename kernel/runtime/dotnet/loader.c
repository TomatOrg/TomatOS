#include "loader.h"

#include "types.h"
#include "runtime/dotnet/gc/gc.h"
#include "encoding.h"
#include "runtime/dotnet/metadata/sig.h"
#include "util/stb_ds.h"

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

static void init_type(metadata_t* metadata, System_Reflection_Assembly Assembly, System_Type type, int index) {
    metadata_type_def_t* type_def = metadata_get_type_def(metadata, index);

    // set the basic type information
    GC_UPDATE(type, Assembly, Assembly);
    GC_UPDATE(type, Module, Assembly->Module);
    GC_UPDATE(type, Name, new_string_from_utf8(type_def->type_name, strlen(type_def->type_name)));
    GC_UPDATE(type, Namespace, new_string_from_utf8(type_def->type_namespace, strlen(type_def->type_namespace)));
    type->Attributes = type_def->flags;

    // handle the inheritance type
    if (type_def->extends.index != 0) {
        GC_UPDATE(type, BaseType, get_type_by_token(Assembly, type_def->extends));
    }
}

static err_t init_type_fields(metadata_t* metadata, System_Reflection_Assembly Assembly, System_Type type, int index) {
    err_t err = NO_ERROR;

    metadata_type_def_t* type_def = metadata_get_type_def(metadata, index);

    // get the fields count
    // TODO: this code is uglyyyyyyyy
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
        GC_UPDATE(fieldInfo, DeclaringType, type);
        GC_UPDATE(fieldInfo, Module, Assembly->Module);
        GC_UPDATE(fieldInfo, Name, new_string_from_utf8(field_def->name, strlen(field_def->name)));

        // parse the field signature
        CHECK_AND_RETHROW(parse_field_sig(field_def->signature, fieldInfo));

        // store it
        GC_UPDATE_ARRAY(type->Fields, i, fieldInfo);
    }

cleanup:
    return err;
}

static err_t init_type_sizes(System_Type Type) {
    err_t err = NO_ERROR;

    // we already initialized the size, continue
    if (Type->SizeValid) {
        CHECK(Type->StackAlignment != 0);
        goto cleanup;
    }

    Type->SizeValid = true;

    size_t alignment = 1;
    size_t size = 0;

    // initialize the size of the base type
    if (Type->BaseType != NULL) {
        CHECK_AND_RETHROW(init_type_sizes(Type->BaseType));

        if (Type->BaseType->IsValueType) {
            Type->IsValueType = true;
            CHECK(Type->BaseType->StackSize == 0);
        }

        alignment = Type->BaseType->ManagedAlignment;
        size = Type->BaseType->ManagedSize;

        // copy the manged pointers from the base
        arrsetlen(Type->ManagedPointersOffsets, arrlen(Type->BaseType->ManagedPointersOffsets));
        memcpy(Type->ManagedPointersOffsets, Type->BaseType->ManagedPointersOffsets,
               arrlen(Type->BaseType->ManagedPointersOffsets) * sizeof(size_t));
    }

    // continue from the last type
    for (int i = 0; Type->Fields != NULL && i < Type->Fields->Length; i++) {
        System_Reflection_FieldInfo FieldInfo = Type->Fields->Data[i];
        System_Type FieldType = FieldInfo->FieldType;

        CHECK_AND_RETHROW(init_type_sizes(FieldType));

        // skip static fields
        if (field_is_static(FieldInfo)) {
            continue;
        }

        // align to the current field
        size = ALIGN_UP(size, FieldType->StackAlignment);

        // check if alignment should be changed
        if (alignment < FieldType->StackAlignment) {
            alignment = FieldType->StackAlignment;
        }

        // set the field offset
        FieldInfo->MemoryOffset = size;

        if (!FieldType->IsValueType) {
            // save this as a managed pointer offset
            arrpush(Type->ManagedPointersOffsets, FieldInfo->MemoryOffset);
        }

        // add to the size
        size += FieldType->StackSize;
    }

    // align the size of the whole thing
    size = ALIGN_UP(size, alignment);

    // for builtin-types verify that their size makes sense
    if (Type->ManagedSize != 0) {
        CHECK(Type->ManagedSize == size,
              "Expected type %U.%U to have size of %d, but got size of %d",
              Type->Namespace, Type->Name, Type->ManagedSize, size);

        CHECK(Type->ManagedAlignment == alignment,
              "Expected type %U.%U to have alignment of %d, but got alignment of %d",
              Type->Namespace, Type->Name, Type->ManagedAlignment, alignment);
    }

    Type->ManagedSize = size;
    Type->ManagedAlignment = alignment;

    if (Type->IsValueType) {
        // this is the same as the managed size
        Type->StackSize = Type->ManagedSize;
        Type->StackAlignment = Type->ManagedAlignment;
    } else {
        // this is the same as a pointer
        Type->StackSize = sizeof(void*);
        Type->StackAlignment = alignof(void*);
    }

cleanup:
    return err;
}

static err_t init_module(metadata_t* metadata, System_Reflection_Assembly Assembly) {
    err_t err = NO_ERROR;

    // make sure we got a single module
    CHECK(metadata->tables[METADATA_MODULE].rows == 1, "The Module table shall contain one and only one row");
    metadata_module_t* md_module = metadata->tables[METADATA_MODULE].table;

    // create the module
    System_Reflection_Module Module = GC_NEW(tSystem_Reflection_Module);
    GC_UPDATE(Module, Assembly, Assembly);
    GC_UPDATE(Module, Name, new_string_from_utf8(md_module->name, strlen(md_module->name)));

    // set it in the assembly
    GC_UPDATE(Assembly, Module, Module);

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
    size_t stack_alignment;
    size_t managed_size;
    size_t managed_alignment;
    bool value_type;
} builtin_type_t;

#define BUILTIN_TYPE(_namespace, _name, code) \
    { .namespace = (_namespace), .name = (_name), &t##code, sizeof(code), alignof(code), sizeof(struct code), alignof(struct code), false }

#define BUILTIN_VALUE_TYPE(_namespace, _name, code) \
    { .namespace = (_namespace), .name = (_name), &t##code, sizeof(code), alignof(code), sizeof(code), alignof(code), true }

static builtin_type_t m_builtin_types[] = {
        BUILTIN_TYPE("System", "ValueType", System_ValueType),
        BUILTIN_TYPE("System", "Object", System_Object),
        BUILTIN_TYPE("System", "Type", System_Type),
        BUILTIN_TYPE("System", "Array", System_Array),
        BUILTIN_TYPE("System", "String", System_String),
        BUILTIN_VALUE_TYPE("System", "Boolean", System_Boolean),
        BUILTIN_VALUE_TYPE("System", "Char", System_Char),
        BUILTIN_VALUE_TYPE("System", "SByte", System_SByte),
        BUILTIN_VALUE_TYPE("System", "Byte", System_Byte),
        BUILTIN_VALUE_TYPE("System", "Int16", System_Int16),
        BUILTIN_VALUE_TYPE("System", "UInt16", System_UInt16),
        BUILTIN_VALUE_TYPE("System", "Int32", System_Int32),
        BUILTIN_VALUE_TYPE("System", "UInt32", System_UInt32),
        BUILTIN_VALUE_TYPE("System", "Int64", System_Int64),
        BUILTIN_VALUE_TYPE("System", "UInt64", System_UInt64),
        BUILTIN_VALUE_TYPE("System", "Single", System_Single),
        BUILTIN_VALUE_TYPE("System", "Double", System_Double),
        BUILTIN_VALUE_TYPE("System", "IntPtr", System_IntPtr),
        BUILTIN_VALUE_TYPE("System", "UIntPtr", System_UIntPtr),
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
            type->ManagedSize = bt->managed_size;
            type->StackSize = bt->stack_size;
            type->ManagedAlignment = bt->managed_alignment;
            type->StackAlignment = bt->stack_alignment;
            if (bt->value_type) {
                type->IsValueType = true;
                type->SizeValid = true;
            }
            *bt->global = type;
            break;
        }
    }
}

System_Reflection_Assembly g_corelib = NULL;

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
        type->ArrayTypeMutex = malloc(sizeof(mutex_t));
        assembly->DefinedTypes->Data[i] = type;
        init_builtin_type(type_def, type);
    }

    // validate we got all the builtin types
    for (int i = 0; i < ARRAY_LEN(m_builtin_types); i++) {
        builtin_type_t* bt = &m_builtin_types[i];
        CHECK(*bt->global != NULL, "Failed to find builtin type `%s.%s`", bt->namespace, bt->name);
    }

    assembly->type = tSystem_Reflection_Assembly;
    for (int i = 0; i < type_count; i++) {
        assembly->DefinedTypes->Data[i]->type = tSystem_Type;
    }

    //
    // now we can continue with loading the rest normally
    //

    // initialize the module itself
    CHECK_AND_RETHROW(init_module(&metadata, assembly));

    // first initialize all the special types we want, in a specific order
    for (int i = 0; i < ARRAY_LEN(m_builtin_types); i++) {
        builtin_type_t* bt = &m_builtin_types[i];
        CHECK(*bt->global != NULL, "Failed to find builtin type `%s.%s`", bt->namespace, bt->name);
    }

    // first phase, initialize the basic type information
    for (int i = 0; i < type_count; i++) {
        // already initialized
        init_type(&metadata, assembly, assembly->DefinedTypes->Data[i], i);
    }

    // we can now safely set the DefinedTypes type
    assembly->DefinedTypes->type = get_array_type(tSystem_Type);

    // second phase, initialize the fields
    for (int i = 0; i < type_count; i++) {
        // already initialized
        init_type_fields(&metadata, assembly, assembly->DefinedTypes->Data[i], i);
    }

    // third phase, initialize the offsets and sizes of everything
    for (int i = 0; i < type_count; i++) {
        // already initialized
        CHECK_AND_RETHROW(init_type_sizes(assembly->DefinedTypes->Data[i]));
    }

    TRACE("Types:");
    for (int i = 0; i < assembly->DefinedTypes->Length; i++) {
        System_Type type = assembly->DefinedTypes->Data[i];
        if (type->BaseType != NULL) {
            TRACE("\t%s %U.%U : %U.%U", type_visibility_str(type_visibility(type)),
                  type->Namespace, type->Name,
                  type->BaseType->Namespace, type->BaseType->Name);
        } else {
            TRACE("\t%s %U.%U", type_visibility_str(type_visibility(type)),
                  type->Namespace, type->Name);
        }

        for (int j = 0; j < type->Fields->Length; j++) {
            CHECK(type->Fields->Data[j] != NULL);
            TRACE("\t\t%s %s%U.%U %U; // offset 0x%02x",
                  field_access_str(field_access(type->Fields->Data[j])),
                  field_is_static(type->Fields->Data[j]) ? "static " : "",
                  type->Fields->Data[j]->FieldType->Namespace,
                  type->Fields->Data[j]->FieldType->Name,
                  type->Fields->Data[j]->Name,
                  type->Fields->Data[j]->MemoryOffset);
        }
    }

    // set the global and add it as a root
    g_corelib = assembly;
    gc_add_root(&g_corelib);

cleanup:
    return err;
}
