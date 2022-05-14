#include "loader.h"
#include "exception.h"
#include "runtime/dotnet/gc/gc.h"
#include "encoding.h"
#include "runtime/dotnet/metadata/sig.h"
#include "util/stb_ds.h"
#include "opcodes.h"
#include "runtime/dotnet/jit/jit.h"

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

static err_t parse_method_cil(metadata_t* metadata, System_Reflection_MethodInfo method, blob_entry_t sig) {
    err_t err = NO_ERROR;

    System_Reflection_MethodBody body = GC_NEW(tSystem_Reflection_MethodBody);

    // set it
    GC_UPDATE(method, MethodBody, body);

    // get the signature table
    metadata_stand_alone_sig_t* standalone_sigs = metadata->tables[METADATA_STAND_ALONE_SIG].table;
    int standalone_sigs_count = metadata->tables[METADATA_STAND_ALONE_SIG].rows;

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

        // skip the rest of the header
        sig.data += header->size * 4;
        sig.size -= header->size * 4;

        // set the init locals flag
        body->InitLocals = header->flags & CorILMethod_InitLocals;

        // variables
        if (header->local_var_sig_tok.token != 0) {
            CHECK(header->local_var_sig_tok.table == METADATA_STAND_ALONE_SIG);
            CHECK(header->local_var_sig_tok.index > 0);
            CHECK(header->local_var_sig_tok.index <= standalone_sigs_count);
            blob_entry_t signature = standalone_sigs[header->local_var_sig_tok.index - 1].signature;
            CHECK_AND_RETHROW(parse_stand_alone_local_var_sig(signature, method));
        } else {
            // empty array for ease of use
            GC_UPDATE(body, LocalVariables, GC_NEW_ARRAY(tSystem_Reflection_LocalVariableInfo, 0));
        }

        // copy some info
        body->MaxStackSize = header->max_stack;

        // copy the il
        CHECK(header->code_size <= sig.size);
        GC_UPDATE(body, Il, GC_NEW_ARRAY(tSystem_Byte, header->code_size));
        memcpy(body->Il->Data, sig.data, body->Il->Length);
        sig.size -= header->code_size;
        sig.data += header->code_size;

        // process method sections
        bool more_sect = header->flags & CorILMethod_MoreSects;
        while (more_sect) {
            // align the data so we can handle the next section
            size_t diff = sig.size - ALIGN_DOWN(sig.size, 4);
            CHECK(diff <= sig.size);
            sig.size -= diff;
            sig.data += diff;

            // get the flags of the section
            CHECK(2 <= sig.size);
            uint8_t flags = sig.data[0];

            // get the section header
            size_t section_size = 0;
            if (flags & CorILMethod_Sect_FatFormat) {
                CHECK(4 <= sig.size);
                method_section_fat_t* section = (method_section_fat_t*) sig.data;
                sig.data += sizeof(method_section_fat_t);
                sig.size -= sizeof(method_section_fat_t);
                section_size = section->size;
            } else {
                method_section_tiny_t* section = (method_section_tiny_t*)sig.data;
                sig.data += sizeof(method_section_tiny_t);
                sig.size -= sizeof(method_section_tiny_t);
                section_size = section->size;
            }

            // verify we have the whole section, so we don't need to worry about it later on
            CHECK(section_size <= sig.size);

            // check the type
            uint8_t kind = flags & CorILMethod_Sect_KindMask;
            switch (kind) {
                case CorILMethod_Sect_EHTable: {
                    CHECK(body->ExceptionHandlingClauses == NULL);
                    size_t count = 0;
                    if (flags & CorILMethod_Sect_FatFormat) {
                        // fat exception table
                        count = (section_size - 4) / 24;
                    } else {
                        // non-fat exception table
                        // skip 2 reserved bytes
                        CHECK(2 <= sig.size);
                        sig.size -= 2;
                        sig.data += 2;
                        count = (section_size - 4) / 12;
                    }

                    // allocate it
                    GC_UPDATE(body, ExceptionHandlingClauses, GC_NEW_ARRAY(tSystem_Reflection_ExceptionHandlingClause, count));

                    // parse it
                    for (int i = 0; i < count; i++) {
                        System_Reflection_ExceptionHandlingClause clause = GC_NEW(tSystem_Reflection_ExceptionHandlingClause);
                        GC_UPDATE_ARRAY(body->ExceptionHandlingClauses, i, clause);

                        if (flags & CorILMethod_Sect_FatFormat) {
                            // fat clause
                            method_fat_exception_clause_t* ec = (method_fat_exception_clause_t*)sig.data;
                            sig.size -= sizeof(method_section_fat_t);
                            sig.data += sizeof(method_section_fat_t);
                            if (ec->flags == COR_ILEXCEPTION_CLAUSE_EXCEPTION) {
                                clause->CatchType = assembly_get_type_by_token(method->Module->Assembly, ec->class_token);
                            } else if (ec->flags == COR_ILEXCEPTION_CLAUSE_FILTER) {
                                clause->FilterOffset = ec->filter_offset;
                            }
                            clause->Flags = ec->flags;
                            clause->HandlerLength = ec->handler_length;
                            clause->HandlerOffset = ec->handler_offset;
                            clause->TryLength = ec->try_length;
                            clause->TryOffset = ec->try_offset;
                        } else {
                            // small clause
                            method_exception_clause_t* ec = (method_exception_clause_t*)sig.data;
                            sig.size -= sizeof(method_exception_clause_t);
                            sig.data += sizeof(method_exception_clause_t);
                            if (ec->flags == COR_ILEXCEPTION_CLAUSE_EXCEPTION) {
                                clause->CatchType = assembly_get_type_by_token(method->Module->Assembly, ec->class_token);
                            } else if (ec->flags == COR_ILEXCEPTION_CLAUSE_FILTER) {
                                clause->FilterOffset = ec->filter_offset;
                            }
                            clause->Flags = ec->flags;
                            clause->HandlerLength = ec->handler_length;
                            clause->HandlerOffset = ec->handler_offset;
                            clause->TryLength = ec->try_length;
                            clause->TryOffset = ec->try_offset;
                        }

                        // check the type
                        // TODO: is this a bit field or not? I can't figure out
                        CHECK(
                            clause->Flags == COR_ILEXCEPTION_CLAUSE_EXCEPTION ||
                            clause->Flags == COR_ILEXCEPTION_CLAUSE_FILTER ||
                            clause->Flags == COR_ILEXCEPTION_CLAUSE_FINALLY ||
                            clause->Flags == COR_ILEXCEPTION_CLAUSE_FAULT
                        );

                        // check offsets
                        CHECK(clause->HandlerOffset < header->code_size);
                        CHECK(clause->HandlerOffset + clause->HandlerLength < header->code_size);
                        CHECK(clause->TryOffset < header->code_size);
                        CHECK(clause->TryOffset + clause->TryLength < header->code_size);

                        // TODO: check for overlaps

                        // make sure handler comes after try
                        CHECK(clause->TryOffset < clause->HandlerOffset);
                    }
                } break;

                default: {
                    CHECK_FAIL("Invalid section kind: %x", kind);
                } break;
            }

            // check for more sections
            more_sect = flags & CorILMethod_Sect_MoreSects;
        }

        // an empty arrays if there are no exceptions
        if (body->ExceptionHandlingClauses == NULL) {
            GC_UPDATE(body, ExceptionHandlingClauses, GC_NEW_ARRAY(tSystem_Reflection_ExceptionHandlingClause, 0));
        }
    } else if ((header_type & 0b11) == CorILMethod_TinyFormat) {
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // tiny format header
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        // the size is known to be good since it is a single byte
        method_tiny_format_t* header = (method_tiny_format_t*) (sig.data);

        // skip the rest of the header
        sig.size--;
        sig.data++;

        // no local variables
        GC_UPDATE(body, LocalVariables, GC_NEW_ARRAY(tSystem_Reflection_LocalVariableInfo, 0));

        // no exceptions
        GC_UPDATE(body, ExceptionHandlingClauses, GC_NEW_ARRAY(tSystem_Reflection_ExceptionHandlingClause, 0));

        // set the default options
        body->MaxStackSize = 8;

        // copy the il
        CHECK(header->size <= sig.size);
        GC_UPDATE(body, Il, GC_NEW_ARRAY(tSystem_Byte, header->size));
        memcpy(body->Il->Data, sig.data, body->Il->Length);
    } else {
        CHECK_FAIL("Invalid method format");
    }

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
                CHECK_AND_RETHROW(parse_method_cil(metadata, methodInfo, (blob_entry_t){
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

static err_t override_method(System_Type type, System_Reflection_MethodInfo method) {
    err_t err = NO_ERROR;

    while (type != NULL) {
        // TODO: search MethodImpl table

        // this type does not have a method table, meaning
        // we can stop our search now
        if (type->VirtualMethods == NULL) {
            break;
        }

        // Use normal inheritance (I.8.10.4)
        for (int i = 0; i < type->VirtualMethods->Length; i++) {
            System_Reflection_MethodInfo info = type->VirtualMethods->Data[i];
            if (!method_is_virtual(info)) continue;

            // match the name
            if (!string_equals(info->Name, method->Name)) continue;

            // if this method is hidden by signature then check the
            // full signature match
            if (method_is_hide_by_sig(info)) {
                // check the return type
                if (info->ReturnType != method->ReturnType) continue;

                // Check parameter count matches
                if (info->Parameters->Length != method->Parameters->Length) continue;

                // check the parameters
                bool signatureMatch = true;
                for (int j = 0; j < info->Parameters->Length; j++) {
                    System_Reflection_ParameterInfo paramA = info->Parameters->Data[j];
                    System_Reflection_ParameterInfo paramB = method->Parameters->Data[j];
                    if (paramA->ParameterType != paramB->ParameterType) {
                        signatureMatch = false;
                        break;
                    }
                }
                if (!signatureMatch) continue;
            }

            // found it!
            CHECK(!method_is_final(info), "Trying to override a final method");

            // set the offset
            method->VtableOffset = info->VtableOffset;
            goto cleanup;
        }

        // get the parent for next iteration
        type = type->BaseType;
    }

    // the vtable slot was not found, we will simply allocate a new slot
    err = ERROR_NOT_FOUND;

cleanup:
    return err;
}

/**
 * Checks if a value type without needing to initialize the full type
 */
static bool unprimed_is_value_type(System_Type type) {
    while (type != NULL) {
        if (type == tSystem_ValueType) return true;
        type = type->BaseType;
    }
    return false;
}

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
    bool need_new_vtable = false;
    int virtualOfs = 0;
    int managedSize = 0;
    int managedSizePrev = 0;
    int managedAlignment = 1;
    if (type->BaseType != NULL) {
        // validate that we don't inherit from a sealed type
        CHECK(!type_is_sealed(type->BaseType));

        if (type->BaseType->IsValueType) {
            // Can not inherit from value types, except for enum which is allowed
            CHECK(type->BaseType == tSystem_ValueType || type->BaseType == tSystem_Enum);
        }

        // fill the type information of the parent
        CHECK_AND_RETHROW(loader_fill_type(type->BaseType, genericTypeArguments, genericMethodArguments));

        // check we have a size
        if (!type->BaseType->IsValueType) {
            CHECK(type->BaseType->ManagedSize != 0);
        }

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
                // we have a virtual method, we must have a new vtable
                need_new_vtable = true;

                if (method_is_new_slot(methodInfo)) {
                    // this is a newslot, always allocate a new slot
                    methodInfo->VtableOffset = virtualOfs++;
                } else {
                    err = override_method(type->BaseType, methodInfo);
                    if (err == ERROR_NOT_FOUND) {
                        // The base method was not found, just allocate a new slot per the spec.
                        methodInfo->VtableOffset = virtualOfs++;
                        err = NO_ERROR;
                    }
                    CHECK_AND_RETHROW(err);
                }
            }
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Virtual Method Table initial creation, the rest will be handled later
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        // we must create the vtable before other type resolution is done because we must
        // have the subtypes know about the amount of virtual entries we have, and we must populate
        // our stuff at the very end
        if (need_new_vtable) {
            // we have our own vtable, if we have a parent with a vtable then copy
            // its vtable entries to our vtable
            type->VirtualMethods = GC_NEW_ARRAY(tSystem_Reflection_MethodInfo, virtualOfs);
            if (type->BaseType != NULL && type->BaseType->VirtualMethods != NULL) {
                for (int i = 0; i < type->BaseType->VirtualMethods->Length; i++) {
                    GC_UPDATE_ARRAY(type->VirtualMethods, i, type->BaseType->VirtualMethods->Data[i]);
                }
            }
        } else {
            // just inherit the vtable
            if (type->BaseType != NULL) {
                type->VirtualMethods = type->BaseType->VirtualMethods;
            }
        }

        // Now fill with our own methods
        for (int i = 0; i < type->Methods->Length; i++) {
            System_Reflection_MethodInfo methodInfo = type->Methods->Data[i];

            if (method_is_virtual(methodInfo)) {
                GC_UPDATE_ARRAY(type->VirtualMethods, methodInfo->VtableOffset, methodInfo);
            }
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // process all the non-static fields at this moment, we are going to calculate the size the
        // same way SysV does it
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        // If its not a value-type and the stack-size is not present, then set it up now.
        // It needs to be done here as non-static fields in non-value types can point to
        // the containing type.
        if (type->StackSize == 0 && !type->IsValueType) {
            type->StackSize = sizeof(void*);
        }

        // for non-static we have two steps, first resolve all the stack sizes, for ref types
        // we are not going to init ourselves yet
        for (int i = 0; i < type->Fields->Length; i++) {
            System_Reflection_FieldInfo fieldInfo = type->Fields->Data[i];
            if (field_is_static(fieldInfo)) continue;

            if (type_is_generic_type(type)) {
                // Clone the type?
                CHECK_FAIL("TODO: Handle generic instantiation");
            }

            // Fill it
            if (unprimed_is_value_type(fieldInfo->FieldType)) {
                CHECK_AND_RETHROW(loader_fill_type(fieldInfo->FieldType, genericTypeArguments, genericMethodArguments));
                CHECK(fieldInfo->FieldType->StackSize != 0);
            } else {
                fieldInfo->FieldType->StackSize = sizeof(void*);
            }

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

            if (type_is_enum(type)) {
                if (string_equals_cstr(fieldInfo->Name, "value__")) {
                    // must be an integer type
                    CHECK(type_is_integer(fieldInfo->FieldType));
                    type->ElementType = fieldInfo->FieldType;
                }
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

        // now that we initialized the instance size of this, we can go over and initialize
        // all the fields, both static and non-static
        for (int i = 0; i < type->Fields->Length; i++) {
            System_Reflection_FieldInfo fieldInfo = type->Fields->Data[i];

            if (type_is_generic_type(type)) {
                // Clone the type?
                CHECK_FAIL("TODO: Handle generic instantiation");
            }

            // Fill it
            CHECK_AND_RETHROW(loader_fill_type(fieldInfo->FieldType, genericTypeArguments, genericMethodArguments));
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // TODO: Handle static fields
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Now handle all the methods
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        for (int i = 0; i < type->Methods->Length; i++) {
            System_Reflection_MethodInfo methodInfo = type->Methods->Data[i];

            if (type_is_generic_type(type)) {
                // Setup this properly
                CHECK_FAIL("TODO: Handle generic instantiation");
            }

            if (method_is_rt_special_name(methodInfo)) {
                // TODO: .ctor
                // TODO: .cctor
            }

            // for performance reason we are not going to have every method have a finalizer
            // but instead we are going to have it virtually virtual
            if (string_equals_cstr(methodInfo->Name, "Finalize")) {
                // check the signature
                if (methodInfo->ReturnType == NULL && methodInfo->Parameters->Length == 0) {
                    CHECK(type->Finalize == NULL);
                    GC_UPDATE(type, Finalize, methodInfo);
                }
            }
        }

        // figure out if a subclass of us has a finalizer
        if (type->Finalize == NULL) {
            System_Type base = type->BaseType;
            while (base != NULL) {
                if (base->Finalize != NULL) {
                    GC_UPDATE(type, Finalize, base->Finalize);
                    break;
                }
                base = base->BaseType;
            }
        }

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

static err_t parse_user_strings(System_Reflection_Assembly assembly, pe_file_t* file) {
    err_t err = NO_ERROR;

    int string_count = 0;

    // count how many strings we have
    blob_entry_t us = {
        .data = file->us,
        .size = file->us_size
    };
    while (us.size != 0) {
        // get the size
        uint32_t string_size;
        CHECK_AND_RETHROW(parse_compressed_integer(&us, &string_size));

        // we got another string
        string_count++;

        // skip this string entry
        CHECK(string_size <= us.size);
        us.size -= string_size;
        us.data += string_size;
    }

    assembly->UserStrings = GC_NEW_ARRAY(tSystem_String, string_count);

    // now create all the strings
    string_count = 0;
    us.data = file->us;
    us.size = file->us_size;
    while (us.size != 0) {
        int offset = file->us_size - us.size;

        // get the size
        uint32_t string_size;
        CHECK_AND_RETHROW(parse_compressed_integer(&us, &string_size));
        CHECK(string_size <= us.size);

        // create the string and store it
        System_String string = GC_NEW_STRING(string_size / 2);
        memcpy(string->Chars, us.data, (string_size / 2) * 2);

        // set the entries in the table and array
        GC_UPDATE_ARRAY(assembly->UserStrings, string_count, string);
        hmput(assembly->UserStringsTable, offset, string);

        // we got another string
        string_count++;

        // skip this string entry
        us.size -= string_size;
        us.data += string_size;
    }

cleanup:
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

#define EXCEPTION_INIT(_namespace, _name, code) \
    { .namespace = (_namespace), .name = (_name), &t##code, sizeof(code), alignof(code), sizeof(struct System_Exception), alignof(struct System_Exception) }

#define VALUE_TYPE_INIT(_namespace, _name, code) \
    { .namespace = (_namespace), .name = (_name), &t##code, sizeof(code), alignof(code), sizeof(code), alignof(code) }

static type_init_t m_type_init[] = {
    TYPE_INIT("System", "Exception", System_Exception),
    VALUE_TYPE_INIT("System", "Enum", System_Enum),
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
    TYPE_INIT("System.Reflection", "LocalVariableInfo", System_Reflection_LocalVariableInfo),
    TYPE_INIT("System.Reflection", "ExceptionHandlingClause", System_Reflection_ExceptionHandlingClause),
    TYPE_INIT("System.Reflection", "MethodBase", System_Reflection_MethodBase),
    TYPE_INIT("System.Reflection", "MethodBody", System_Reflection_MethodBody),
    TYPE_INIT("System.Reflection", "MethodInfo", System_Reflection_MethodInfo),
    EXCEPTION_INIT("System", "ArithmeticException", System_ArithmeticException),
    EXCEPTION_INIT("System", "DivideByZeroException", System_DivideByZeroException),
    EXCEPTION_INIT("System", "ExecutionEngineException", System_ExecutionEngineException),
    EXCEPTION_INIT("System", "IndexOutOfRangeException", System_IndexOutOfRangeException),
    EXCEPTION_INIT("System", "NullReferenceException", System_NullReferenceException),
    EXCEPTION_INIT("System", "OutOfMemoryException", System_OutOfMemoryException),
    EXCEPTION_INIT("System", "OverflowException", System_OverflowException),
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

static err_t validate_have_init_types() {
    err_t err = NO_ERROR;

    bool missing = false;
    for (int i = 0; i < ARRAY_LEN(m_type_init); i++) {
        type_init_t* bt = &m_type_init[i];
        if (*bt->global == NULL) {
            TRACE("Missing `%s.%s`!", bt->namespace, bt->name);
            missing = true;
        }
    }
    CHECK(!missing);

cleanup:
    return err;
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

    // validate we got all the base types we need for a proper runtime
    CHECK_AND_RETHROW(validate_have_init_types());

    // create the module
    CHECK(metadata.tables[METADATA_MODULE].rows == 1);
    metadata_module_t* module = metadata.tables[METADATA_MODULE].table;
    assembly->Module = GC_NEW(tSystem_Reflection_Module);
    assembly->Module->Name = new_string_from_cstr(module->name);
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

    // now get all the user strings into our pool
    CHECK_AND_RETHROW(parse_user_strings(assembly, &file));

    // now jit it (or well, prepare the ir of it)
    CHECK_AND_RETHROW(jit_assembly(assembly));

    // get the entry point
    GC_UPDATE(assembly, EntryPoint, assembly_get_method_by_token(assembly, file.cli_header->entry_point_token));

    // save this
    g_corelib = assembly;
    gc_add_root(&g_corelib);

cleanup:
    free_metadata(&metadata);
    free_pe_file(&file);

    return err;
}
