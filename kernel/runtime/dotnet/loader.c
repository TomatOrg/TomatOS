#include "loader.h"
#include "exception.h"
#include "runtime/dotnet/gc/gc.h"
#include "encoding.h"
#include "runtime/dotnet/metadata/sig.h"
#include "util/stb_ds.h"

#include <util/string.h>
#include <mem/mem.h>

System_Reflection_Assembly g_corelib = NULL;

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
// All the basic type setup
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static err_t parse_method_cil(System_Reflection_MethodInfo methodBase, blob_entry_t sig) {
    err_t err = NO_ERROR;

    System_Reflection_MethodBody body = GC_NEW(tSystem_Reflection_MethodBody);

    // get the header type
    CHECK(sig.size > 0);
    uint8_t header_type = sig.data[0];

    if ((header_type & 0b11) == CorILMethod_FatFormat) {
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // fat format header
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        // fetch the header in its full
        method_fat_format_t* header = (method_fat_format_t*) (sig.data);
        CHECK(sizeof(method_fat_format_t) <= sig.size);
        CHECK(header->size * 4 <= sig.size);

        // TODO: variables

        // TODO: exceptions

        // skip the rest of the header
        sig.size -= header->size * 4;
        sig.data += header->size * 4;

        // copy some info
        body->MaxStackSize = header->max_stack;

        // copy the il
        CHECK(header->code_size <= sig.size);
        GC_UPDATE(body, Il, GC_NEW_ARRAY(tSystem_Byte, header->code_size));
        memcpy(body->Il->Data, sig.data, body->Il->Length);

    } else if ((header_type & 0b11) == CorILMethod_TinyFormat) {
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // tiny format header
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        // the size is known to be good since it is a single byte
        method_tiny_format_t* header = (method_tiny_format_t*) (sig.data);

        // skip the rest of the header
        sig.size--;
        sig.data++;

        // set the default options
        body->MaxStackSize = 8;

        // copy the il
        CHECK(header->size <= sig.size);
        GC_UPDATE(body, Il, GC_NEW_ARRAY(tSystem_Byte, header->size));
        memcpy(body->Il->Data, sig.data, body->Il->Length);
    } else {
        CHECK_FAIL("Invalid method format");
    }

    // set it
    GC_UPDATE(methodBase, MethodBody, body);

cleanup:
    return err;
}

static err_t setup_type_info(pe_file_t* file, metadata_t* metadata, System_Reflection_Assembly assembly) {
    err_t err = NO_ERROR;

    int types_count = metadata->tables[METADATA_TYPE_DEF].rows;
    metadata_type_def_t* type_defs = metadata->tables[METADATA_TYPE_DEF].table;

    int fields_count = metadata->tables[METADATA_FIELD].rows;
    int methods_count = metadata->tables[METADATA_METHOD_DEF].rows;

    for (int i = 0; i < types_count; i++) {
        metadata_type_def_t* type_def = &type_defs[i];
        System_Type type = assembly->DefinedTypes->Data[i];

        // make sure the type index is valid
        CHECK(type_def->extends.index - 1 < types_count);

        // set the owners and flags
        GC_UPDATE(type, Assembly, assembly);
        GC_UPDATE(type, Module, assembly->Module);
        type->Attributes = type_def->flags;

        // setup the name and base types
        GC_UPDATE(type, Name, new_string_from_utf8(type_def->type_name, strlen(type_def->type_name)));
        GC_UPDATE(type, Namespace, new_string_from_utf8(type_def->type_namespace, strlen(type_def->type_namespace)));
        GC_UPDATE(type, BaseType, assembly_get_type_by_token(assembly, type_def->extends));
    }

    // all the base info is done, now we can do the rest
    for (int i = 0; i < types_count; i++) {
        metadata_type_def_t* type_def = &type_defs[i];
        System_Type type = assembly->DefinedTypes->Data[i];

        // setup fields
        int last_idx = (i + 1 == types_count) ?
                fields_count :
                type_def[1].field_list.index - 1;
        CHECK(last_idx <= fields_count);

        type->Fields = GC_NEW_ARRAY(tSystem_Reflection_FieldInfo, last_idx - type_def->field_list.index + 1);
        for (int fi = 0; fi < type->Fields->Length; fi++) {
            size_t index = type_def->field_list.index + fi - 1;
            metadata_field_t* field = metadata_get_field(metadata, index);
            System_Reflection_FieldInfo fieldInfo = GC_NEW(tSystem_Reflection_FieldInfo);
            GC_UPDATE_ARRAY(type->Fields, fi, fieldInfo);
            GC_UPDATE_ARRAY(assembly->DefinedFields, index, fieldInfo);

            GC_UPDATE(fieldInfo, DeclaringType, type);
            GC_UPDATE(fieldInfo, Module, type->Module);
            GC_UPDATE(fieldInfo, Name, new_string_from_utf8(field->name, strlen(field->name)));
            fieldInfo->Attributes = field->flags;

            CHECK_AND_RETHROW(parse_field_sig(field->signature, fieldInfo));
        }

        // setup fields
        last_idx = (i + 1 == types_count) ?
                       methods_count :
                       type_def[1].method_list.index - 1;
        CHECK(last_idx <= methods_count);

        type->Methods = GC_NEW_ARRAY(tSystem_Reflection_MethodInfo, last_idx - type_def->method_list.index + 1);
        for (int mi = 0; mi < type->Methods->Length; mi++) {
            size_t index = type_def->method_list.index + mi - 1;
            metadata_method_def_t* method_def = metadata_get_method_def(metadata, index);
            System_Reflection_MethodInfo methodInfo = GC_NEW(tSystem_Reflection_MethodInfo);
            GC_UPDATE_ARRAY(type->Methods, mi, methodInfo);
            GC_UPDATE_ARRAY(assembly->DefinedMethods, index, methodInfo);

            GC_UPDATE(methodInfo, DeclaringType, type);
            GC_UPDATE(methodInfo, Module, type->Module);
            GC_UPDATE(methodInfo, Name, new_string_from_utf8(method_def->name, strlen(method_def->name)));
            methodInfo->Attributes = method_def->flags;
            methodInfo->ImplAttributes = method_def->impl_flags;

            if (method_def->rva) {
                // get the rva
                pe_directory_t directory = {
                        .rva = method_def->rva,
                };
                const void* rva_base = pe_get_rva_ptr(file, &directory);
                CHECK(rva_base != NULL);

                // parse the method info
                CHECK_AND_RETHROW(parse_method_cil(methodInfo, (blob_entry_t){
                    .size = directory.size,
                    .data= rva_base
                }));
            }

            CHECK_AND_RETHROW(parse_stand_alone_method_sig(method_def->signature, methodInfo));
        }
    }

cleanup:
    return err;
}

err_t loader_fill_method(System_Type type, System_Reflection_MethodInfo method, System_Type_Array genericTypeArguments, System_Type_Array genericMethodArguments) {
    err_t err = NO_ERROR;

    // don't initialize twice
    if (method->IsFilled) {
        goto cleanup;
    }
    method->IsFilled = true;

    // init return type
    if (method->ReturnType != NULL) {
        CHECK_AND_RETHROW(loader_fill_type(method->ReturnType, genericTypeArguments, genericMethodArguments));
    }

    // init all the other parameters
    for (int i = 1; i < method->Parameters->Length; i++) {
        System_Reflection_ParameterInfo parameterInfo = method->Parameters->Data[i];
        CHECK_AND_RETHROW(loader_fill_type(parameterInfo->ParameterType, genericTypeArguments, genericMethodArguments));
    }

cleanup:
    return err;
}

#if 0
    #define TRACE_FILL_TYPE(...) TRACE(__VA_ARGS__)
#else
    #define TRACE_FILL_TYPE(...)
#endif

err_t loader_fill_type(System_Type type, System_Type_Array genericTypeArguments, System_Type_Array genericMethodArguments) {
    err_t err = NO_ERROR;
    static int depth = 0;
    TRACE_FILL_TYPE("%*s%U.%U", depth * 4, "", type->Namespace, type->Name);
    depth++;

    // the type is already filled, ignore it
    if (type->IsFilled) {
        goto cleanup;
    }

    // we are going to fill the type now
    type->IsFilled = true;

    // special case, we should not have anything else in here that is
    // important specifically for ValueType class
    if (type == tSystem_ValueType) {
        type->IsValueType = true;
        goto cleanup;
    }

    // first check the parent
    int virtualOfs = 0;
    int managedSize = 0;
    int managedSizePrev = 0;
    int managedAlignment = 1;
    if (type->BaseType != NULL) {
        // fill the type information of the parent
        CHECK_AND_RETHROW(loader_fill_type(type->BaseType, genericTypeArguments, genericMethodArguments));

        // now check if it has virtual methods
        if (type->BaseType->VirtualMethods != NULL) {
            virtualOfs = type->BaseType->VirtualMethods->Length;
        }

        // get the managed size
        managedSize = type->BaseType->ManagedSize;
        managedSizePrev = managedSize;
        managedAlignment = type->BaseType->ManagedAlignment;

        // copy the managed pointers offsets
        for (int i = 0; i < arrlen(type->BaseType->ManagedPointersOffsets); i++) {
            arrpush(type->ManagedPointersOffsets, type->BaseType->ManagedPointersOffsets[i]);
        }
    }

    // Set the value type
    if (type->BaseType != NULL && type->BaseType->IsValueType) {
        type->IsValueType = true;
    }

    // make sure this was primed already
    CHECK(type->Methods != NULL);
    CHECK(type->Fields != NULL);

    // this is only needed for non-generic types
    if (!type_is_generic_definition(type)) {
        // first we need to take care of the virtual method table
        for (int i = 0; i < type->Methods->Length; i++) {
            System_Reflection_MethodInfo methodInfo = type->Methods->Data[i];

            if (method_is_virtual(methodInfo)) {
                if (method_is_new_slot(methodInfo) || type->BaseType == NULL) {
                    // this is a newslot or a base type, allocate
                    // a new vtable slot
                    methodInfo->VtableOffset = virtualOfs++;
                } else {
                    CHECK_FAIL("TODO: implement method overriding");
                }
            }
        }

        // If its not a value-type and the stack-size is not present, then set it up now.
        // It needs to be done here as non-static fields in non-value types can point to
        // the containing type.
        if (type->StackSize == 0 && !type->IsValueType) {
            type->StackSize = sizeof(void*);
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // process all the non-static fields at this moment, we are going to calculate the size the
        // same way SysV does it
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        for (int i = 0; i < type->Fields->Length; i++) {
            System_Reflection_FieldInfo fieldInfo = type->Fields->Data[i];
            if (field_is_static(fieldInfo)) continue;

            if (type_is_generic_type(type)) {
                // Clone the type?
                CHECK_FAIL("TODO: Handle generic instantiation");
            }

            // Fill it
            CHECK_AND_RETHROW(loader_fill_type(fieldInfo->FieldType, genericTypeArguments, genericMethodArguments));

            if (field_is_literal(fieldInfo)) {
                CHECK_FAIL("TODO: Handle literal or rva");
            } else {
                // align the offset, set it, and then increment by the field size
                managedSize = ALIGN_UP(managedSize, fieldInfo->FieldType->StackAlignment);
                fieldInfo->MemoryOffset = managedSize;
                managedSize += fieldInfo->FieldType->StackSize;
                CHECK(managedSize > managedSizePrev, "Type size overflow! %d -> %d", managedSizePrev, managedSize);
                managedSizePrev = managedSize;

                // pointer offsets for gc
                if (!fieldInfo->FieldType->IsValueType) {
                    // this is a normal reference type, just add the offset to us
                    arrpush(type->ManagedPointersOffsets, fieldInfo->MemoryOffset);
                } else {
                    // for value types we are essentially embedding them in us, so we are
                    // going to just copy all the offsets from them and add their base to
                    // our offsets
                    int* offsets = arraddnptr(type->ManagedPointersOffsets, arrlen(fieldInfo->FieldType->ManagedPointersOffsets));
                    for (int j = 0; j < arrlen(fieldInfo->FieldType->ManagedPointersOffsets); j++, offsets++) {
                        int offset = fieldInfo->FieldType->ManagedPointersOffsets[j];
                        *offsets = (int)fieldInfo->MemoryOffset + offset;
                    }
                }

                // set new type alignment
                managedAlignment = MAX(managedAlignment, fieldInfo->FieldType->StackAlignment);
            }
        }

        // lastly align the whole size to the struct alignment
        managedSize = ALIGN_UP(managedSize, managedAlignment);
        CHECK(managedSize >= managedSizePrev, "Type size overflow! %d >= %d", managedSize, managedSizePrev);

        if (type->ManagedSize != 0) {
            // This has a C type equivalent, verify the sizes match
            CHECK(type->ManagedSize == managedSize && type->ManagedAlignment == managedAlignment,
                  "Size mismatch for type %U.%U (native: %d bytes (%d), dotnet: %d bytes (%d))",
                      type->Namespace, type->Name,
                      type->ManagedSize, type->ManagedAlignment,
                      managedSize, managedAlignment);
        }
        type->ManagedSize = managedSize;
        type->ManagedAlignment = managedAlignment;

        // Sort the stack size, if it was a reference type we already set it, otherwise it
        // is a struct type
        if (type->StackSize == 0) {
            type->StackSize = type->ManagedSize;
            type->StackAlignment = type->ManagedAlignment;
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // TODO: Handle static fields
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Now handle all the methods
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        // setup the new virtual method table
        type->VirtualMethods = GC_NEW_ARRAY(tSystem_Reflection_MethodInfo, virtualOfs);
        if (type->BaseType != NULL && type->BaseType->VirtualMethods != NULL) {
            // copy the virtual table base
            for (int i = 0; i < type->BaseType->VirtualMethods->Length; i++) {
                GC_UPDATE_ARRAY(type->VirtualMethods, i, type->BaseType->VirtualMethods->Data[i]);
            }
        }

        for (int i = 0; i < type->Methods->Length; i++) {
            System_Reflection_MethodInfo methodInfo = type->Methods->Data[i];

            if (type_is_generic_type(type)) {
                // Setup this properly
                CHECK_FAIL("TODO: Handle generic instantiation");
            }

            // TODO: figure finalizer and cctor

            // Insert to vtable if needed
            if (method_is_virtual(methodInfo)) {
                GC_UPDATE_ARRAY(type->VirtualMethods, methodInfo->VtableOffset, methodInfo);
            }
        }

        // TODO: Finalizer inheritance handling

        // Now fill all the method defs
        for (int i = 0; i < type->Methods->Length; i++) {
            System_Reflection_MethodInfo methodInfo = type->Methods->Data[i];
            CHECK_AND_RETHROW(loader_fill_method(type, methodInfo, genericTypeArguments, genericMethodArguments));
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // TODO: interface handling
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////



    } else {
        CHECK_FAIL("TODO: Handle generic definitions");
    }

    // set the namespace if this is a nested type
    if (type->DeclaringType != NULL) {
        System_Type rootType = type->DeclaringType;
        while (rootType->DeclaringType != NULL) {
            rootType = rootType->DeclaringType;
        }
        GC_UPDATE(type, Namespace, rootType->Namespace);
    }

cleanup:
    depth--;
    TRACE_FILL_TYPE("%*s%U.%U - %d, %d", depth * 4, "", type->Namespace, type->Name, type->ManagedSize, type->StackSize);
    return err;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Type init
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct type_init {
    const char* namespace;
    const char* name;
    System_Type* global;
    int stack_size;
    int stack_alignment;
    int managed_size;
    int managed_alignment;
    bool value_type;
} type_init_t;

#define TYPE_INIT(_namespace, _name, code) \
    { .namespace = (_namespace), .name = (_name), &t##code, sizeof(code), alignof(code), sizeof(struct code), alignof(struct code) }

#define VALUE_TYPE_INIT(_namespace, _name, code) \
    { .namespace = (_namespace), .name = (_name), &t##code, sizeof(code), alignof(code), sizeof(code), alignof(code) }

static type_init_t m_type_init[] = {
    TYPE_INIT("System", "Exception", System_Exception),
    VALUE_TYPE_INIT("System", "ValueType", System_ValueType),
    TYPE_INIT("System", "Object", System_Object),
    TYPE_INIT("System", "Type", System_Type),
    TYPE_INIT("System", "Array", System_Array),
    TYPE_INIT("System", "String", System_String),
    VALUE_TYPE_INIT("System", "Boolean", System_Boolean),
    VALUE_TYPE_INIT("System", "Char", System_Char),
    VALUE_TYPE_INIT("System", "SByte", System_SByte),
    VALUE_TYPE_INIT("System", "Byte", System_Byte),
    VALUE_TYPE_INIT("System", "Int16", System_Int16),
    VALUE_TYPE_INIT("System", "UInt16", System_UInt16),
    VALUE_TYPE_INIT("System", "Int32", System_Int32),
    VALUE_TYPE_INIT("System", "UInt32", System_UInt32),
    VALUE_TYPE_INIT("System", "Int64", System_Int64),
    VALUE_TYPE_INIT("System", "UInt64", System_UInt64),
    VALUE_TYPE_INIT("System", "Single", System_Single),
    VALUE_TYPE_INIT("System", "Double", System_Double),
    VALUE_TYPE_INIT("System", "IntPtr", System_IntPtr),
    VALUE_TYPE_INIT("System", "UIntPtr", System_UIntPtr),
    TYPE_INIT("System.Reflection", "Module", System_Reflection_Module),
    TYPE_INIT("System.Reflection", "Assembly", System_Reflection_Assembly),
    TYPE_INIT("System.Reflection", "FieldInfo", System_Reflection_FieldInfo),
    TYPE_INIT("System.Reflection", "ParameterInfo", System_Reflection_ParameterInfo),
    TYPE_INIT("System.Reflection", "MethodBase", System_Reflection_MethodBase),
    TYPE_INIT("System.Reflection", "MethodBody", System_Reflection_MethodBody),
    TYPE_INIT("System.Reflection", "MethodInfo", System_Reflection_MethodInfo),
};

static void init_type(metadata_type_def_t* type_def, System_Type type) {
    // check if this is a builtin type
    for (int i = 0; i < ARRAY_LEN(m_type_init); i++) {
        type_init_t* bt = &m_type_init[i];
        if (
            strcmp(type_def->type_namespace, bt->namespace) == 0 &&
            strcmp(type_def->type_name, bt->name) == 0
        ) {
            type->ManagedSize = bt->managed_size;
            type->StackSize = bt->stack_size;
            type->ManagedAlignment = bt->managed_alignment;
            type->StackAlignment = bt->stack_alignment;
            *bt->global = type;
            break;
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// corelib is a bit different so load it as needed
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

err_t loader_load_corelib(void* buffer, size_t buffer_size) {
    err_t err = NO_ERROR;
    metadata_t metadata = { 0 };

    // Start by loading the PE file for the corelib
    pe_file_t file = {
        .file = buffer,
        .file_size = buffer_size
    };
    CHECK_AND_RETHROW(pe_parse(&file));

    // decode the dotnet metadata
    CHECK_AND_RETHROW(decode_metadata(&file, &metadata));

    // allocate the corelib on the kernel heap and not the object heap, just because
    // it is always going to be allocated anyways
    System_Reflection_Assembly assembly = malloc(sizeof(struct System_Reflection_Assembly));
    CHECK(assembly != NULL);

    // setup the basic type system
    int types_count = metadata.tables[METADATA_TYPE_DEF].rows;
    metadata_type_def_t* type_defs = metadata.tables[METADATA_TYPE_DEF].table;

    int method_count = metadata.tables[METADATA_METHOD_DEF].rows;
    int field_count = metadata.tables[METADATA_FIELD].rows;

    // do first time allocation and init
    assembly->DefinedTypes = gc_new(NULL, sizeof(struct System_Array) + types_count * sizeof(System_Type));
    assembly->DefinedTypes->Length = types_count;
    for (int i = 0; i < types_count; i++) {
        metadata_type_def_t* type_def = &type_defs[i];
        assembly->DefinedTypes->Data[i] = gc_new(NULL, sizeof(struct System_Type));
        CHECK(assembly->DefinedTypes->Data[i] != NULL);
        init_type(type_def, assembly->DefinedTypes->Data[i]);
    }

    // create the module
    assembly->Module = GC_NEW(tSystem_Reflection_Module);
    assembly->Module->Assembly = assembly;

    assembly->DefinedMethods = gc_new(NULL, sizeof(struct System_Array) + method_count * sizeof(System_Reflection_MethodInfo));
    assembly->DefinedMethods->Length = method_count;
    assembly->DefinedFields = gc_new(NULL, sizeof(struct System_Array) + field_count * sizeof(System_Reflection_FieldInfo));
    assembly->DefinedFields->Length = field_count;

    // do first time type init
    CHECK_AND_RETHROW(setup_type_info(&file, &metadata, assembly));

    // initialize all the types we have
    for (int i = 0; i < types_count; i++) {
        CHECK_AND_RETHROW(loader_fill_type(assembly->DefinedTypes->Data[i], NULL, NULL));
    }

    // now set the base definitions for the stuff
    assembly->type = tSystem_Reflection_Assembly;
    assembly->DefinedTypes->type = get_array_type(tSystem_Type);
    assembly->DefinedMethods->type = get_array_type(tSystem_Reflection_MethodInfo);
    assembly->DefinedFields->type = get_array_type(tSystem_Reflection_FieldInfo);
    for (int i = 0; i < types_count; i++) {
        assembly->DefinedTypes->Data[i]->type = tSystem_Type;
    }

    // save this
    g_corelib = assembly;
    gc_add_root(&g_corelib);

cleanup:
    free_metadata(&metadata);
    free_pe_file(&file);

    return err;
}
