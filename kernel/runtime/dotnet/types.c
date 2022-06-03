#include "types.h"
#include "opcodes.h"

#include <runtime/dotnet/gc/gc.h>
#include <mem/mem.h>

#include <util/stb_ds.h>
#include <util/string.h>
#include <util/strbuilder.h>

System_Type tSystem_Enum = NULL;
System_Type tSystem_Exception = NULL;
System_Type tSystem_ValueType = NULL;
System_Type tSystem_Object = NULL;
System_Type tSystem_Type = NULL;
System_Type tSystem_Array = NULL;
System_Type tSystem_String = NULL;
System_Type tSystem_Boolean = NULL;
System_Type tSystem_Char = NULL;
System_Type tSystem_SByte = NULL;
System_Type tSystem_Byte = NULL;
System_Type tSystem_Int16 = NULL;
System_Type tSystem_UInt16 = NULL;
System_Type tSystem_Int32 = NULL;
System_Type tSystem_UInt32 = NULL;
System_Type tSystem_Int64 = NULL;
System_Type tSystem_UInt64 = NULL;
System_Type tSystem_Single = NULL;
System_Type tSystem_Double = NULL;
System_Type tSystem_IntPtr = NULL;
System_Type tSystem_UIntPtr = NULL;
System_Type tSystem_Reflection_Module = NULL;
System_Type tSystem_Reflection_Assembly = NULL;
System_Type tSystem_Reflection_FieldInfo = NULL;
System_Type tSystem_Reflection_MemberInfo = NULL;
System_Type tSystem_Reflection_ParameterInfo = NULL;
System_Type tSystem_Reflection_LocalVariableInfo = NULL;
System_Type tSystem_Reflection_ExceptionHandlingClause = NULL;
System_Type tSystem_Reflection_MethodBase = NULL;
System_Type tSystem_Reflection_MethodBody = NULL;
System_Type tSystem_Reflection_MethodInfo = NULL;
System_Type tSystem_ArithmeticException = NULL;
System_Type tSystem_DivideByZeroException = NULL;
System_Type tSystem_ExecutionEngineException = NULL;
System_Type tSystem_IndexOutOfRangeException = NULL;
System_Type tSystem_NullReferenceException = NULL;
System_Type tSystem_InvalidCastException = NULL;
System_Type tSystem_OutOfMemoryException = NULL;
System_Type tSystem_OverflowException = NULL;

System_Type tPentagon_Reflection_InterfaceImpl = NULL;

bool string_equals_cstr(System_String a, const char* b) {
    if (a->Length != strlen(b)) {
        return false;
    }

    for (int i = 0; i < a->Length; i++) {
        if (a->Chars[i] != b[i]) {
            return false;
        }
    }

    return true;
}

bool string_equals(System_String a, System_String b) {
    if (a == b) {
        return true;
    }

    if (a->Length != b->Length) {
        return false;
    }

    for (int i = 0; i < a->Length; i++) {
        if (a->Chars[i] != b->Chars[i]) {
            return false;
        }
    }

    return true;
}

System_String string_append_cstr(System_String old, const char* str) {
    size_t len = strlen(str);

    // copy the old chars
    System_String new = GC_NEW_STRING(old->Length + len);
    memcpy(new->Chars, old->Chars, sizeof(System_Char) * old->Length);

    // copy the new chars
    for (int i = 0; i < len; i++) {
        new->Chars[old->Length + i] = str[i];
    }

    return new;
}

System_Type assembly_get_type_by_token(System_Reflection_Assembly assembly, token_t token) {
    if (token.index == 0) {
        // null token is valid for our case
        return NULL;
    }

    switch (token.table) {
        case METADATA_TYPE_DEF: {
            if (token.index - 1 >= assembly->DefinedTypes->Length) {
                ASSERT(!"assembly_get_type_by_token: token outside of range");
                return NULL;
            }
            return assembly->DefinedTypes->Data[token.index - 1];
        } break;

        case METADATA_TYPE_REF: {
            if (token.index - 1 >= assembly->ImportedTypes->Length) {
                ASSERT(!"assembly_get_type_by_token: token outside of range");
                return NULL;
            }
            return assembly->ImportedTypes->Data[token.index - 1];
        } break;

        case METADATA_TYPE_SPEC: {
            ASSERT(!"assembly_get_type_by_token: TODO: TypeSpec");
        } break;

        default:
            ASSERT(!"assembly_get_type_by_token: invalid table for type");
            return NULL;
    }
}

System_Reflection_MethodInfo assembly_get_method_by_token(System_Reflection_Assembly assembly, token_t token) {
    if (token.index == 0) {
        // null token is valid for our case
        return NULL;
    }

    switch (token.table) {
        case METADATA_METHOD_DEF: {
            if (token.index - 1 >= assembly->DefinedMethods->Length) {
                ASSERT(!"assembly_get_method_by_token: token outside of range");
                return NULL;
            }
            return assembly->DefinedMethods->Data[token.index - 1];
        } break;

        case METADATA_MEMBER_REF: {
            if (token.index - 1 >= assembly->ImportedMembers->Length) {
                ASSERT(!"assembly_get_method_by_token: token outside of range");
                return NULL;
            }
            System_Reflection_MemberInfo memberInfo = assembly->ImportedMembers->Data[token.index - 1];
            if (memberInfo->vtable->type != tSystem_Reflection_MethodInfo) {
                ASSERT(!"assembly_get_method_by_token: wanted member is not a method");
                return NULL;
            }
            return (System_Reflection_MethodInfo)memberInfo;
        } break;

        default:
            ASSERT(!"assembly_get_method_by_token: invalid table for type");
            return NULL;
    }
}

System_Reflection_FieldInfo assembly_get_field_by_token(System_Reflection_Assembly assembly, token_t token) {
    if (token.index == 0) {
        // null token is valid for our case
        return NULL;
    }

    switch (token.table) {
        case METADATA_FIELD: {
            if (token.index - 1 >= assembly->DefinedFields->Length) {
                ASSERT(!"assembly_get_field_by_token: token outside of range");
                return NULL;
            }
            return assembly->DefinedFields->Data[token.index - 1];
        } break;

        case METADATA_MEMBER_REF: {
            if (token.index - 1 >= assembly->ImportedMembers->Length) {
                ASSERT(!"assembly_get_field_by_token: token outside of range");
                return NULL;
            }
            System_Reflection_MemberInfo memberInfo = assembly->ImportedMembers->Data[token.index - 1];
            if (memberInfo->vtable->type != tSystem_Reflection_FieldInfo) {
                ASSERT(!"assembly_get_field_by_token: wanted member is not a field");
                return NULL;
            }
            return (System_Reflection_FieldInfo)memberInfo;
        } break;

        default:
            ASSERT(!"assembly_get_field_by_token: invalid table for type");
            return NULL;
    }
}

System_Type assembly_get_type_by_name(System_Reflection_Assembly assembly, const char* name, const char* namespace) {
    for (int i = 0; i < assembly->DefinedTypes->Length; i++) {
        System_Type type = assembly->DefinedTypes->Data[i];
        if (string_equals_cstr(type->Namespace, namespace) && string_equals_cstr(type->Name, name)) {
            return type;
        }
    }
    return NULL;
}

System_String assembly_get_string_by_token(System_Reflection_Assembly assembly, token_t token) {
    if (token.table != 0x70) {
        ASSERT(!"assembly_get_string_by_token: invalid table for type");
        return NULL;
    }
    return hmget(assembly->UserStringsTable, token.index);
}

System_Type get_array_type(System_Type Type) {
    if (Type->ArrayType != NULL) {
        return Type->ArrayType;
    }

    mutex_lock(&Type->TypeMutex);

    if (Type->ArrayType != NULL) {
        mutex_unlock(&Type->TypeMutex);
        return Type->ArrayType;
    }

    // allocate the new type
    System_Type ArrayType = GC_NEW(tSystem_Type);

    // make sure this was called after system array was initialized
    ASSERT(tSystem_Array->Assembly != NULL);

    // set the type information to look as Type[]
    GC_UPDATE(ArrayType, Module, Type->Module);
    GC_UPDATE(ArrayType, Name, string_append_cstr(Type->Name, "[]"));
    GC_UPDATE(ArrayType, Assembly, Type->Assembly);
    GC_UPDATE(ArrayType, BaseType, tSystem_Array);
    GC_UPDATE(ArrayType, Namespace, Type->Namespace);

    // this is an array
    ArrayType->IsArray = true;
    ArrayType->IsFilled = true;
    ArrayType->StackType = STACK_TYPE_O;

    // set the sizes properly
    ArrayType->StackSize = tSystem_Array->StackSize;
    ArrayType->ManagedSize = tSystem_Array->ManagedSize;
    ArrayType->StackAlignment = tSystem_Array->StackAlignment;
    ArrayType->ManagedAlignment = tSystem_Array->ManagedAlignment;

    // allocate the vtable
    ArrayType->VTable = malloc(sizeof(object_vtable_t) + sizeof(void*) * 3);
    ArrayType->VTable->type = ArrayType;

    // There are no managed pointers in here (The gc will handle array
    // stuff on its own)
    ArrayType->ManagedPointersOffsets = NULL;

    // Set the element type
    GC_UPDATE(ArrayType, ElementType, Type);

    // Set the array type
    GC_UPDATE(Type, ArrayType, ArrayType);
    mutex_unlock(&Type->TypeMutex);

    return Type->ArrayType;
}

System_Type get_by_ref_type(System_Type Type) {
    if (Type->ByRefType != NULL) {
        return Type->ByRefType;
    }

    mutex_lock(&Type->TypeMutex);

    if (Type->ByRefType != NULL) {
        mutex_unlock(&Type->TypeMutex);
        return Type->ByRefType;
    }

    // must not be a byref
    ASSERT(!Type->IsByRef);

    // allocate the new ref type
    System_Type ByRefType = GC_NEW(tSystem_Type);

    // this is an array
    ByRefType->IsByRef = 1;
    ByRefType->IsFilled = 1;
    ByRefType->StackType = STACK_TYPE_REF;

    // set the type information to look as ref Type
    GC_UPDATE(ByRefType, Module, Type->Module);
    GC_UPDATE(ByRefType, Name, string_append_cstr(Type->Name, "&"));
    GC_UPDATE(ByRefType, Assembly, Type->Assembly);
    GC_UPDATE(ByRefType, Namespace, Type->Namespace);
    GC_UPDATE(ByRefType, BaseType, Type);

    // set the sizes properly
    ByRefType->StackSize = sizeof(void*);
    ByRefType->ManagedSize = Type->StackSize;
    ByRefType->StackAlignment = alignof(void*);
    ByRefType->ManagedAlignment = Type->StackAlignment;

    // Set the array type
    GC_UPDATE(Type, ByRefType, ByRefType);
    mutex_unlock(&Type->TypeMutex);

    return Type->ByRefType;
}

const char* field_access_str(field_access_t access) {
    static const char* strs[] = {
        [FIELD_COMPILER_CONTROLLED] = "compilercontrolled",
        [FIELD_PRIVATE] = "private",
        [FIELD_FAMILY_AND_ASSEMBLY] = "private protected",
        [FIELD_ASSEMBLY] = "internal",
        [FIELD_FAMILY] = "protected",
        [FIELD_FAMILY_OR_ASSEMBLY] = "protected internal",
        [FIELD_PUBLIC] = "public",
    };
    return strs[access];
}

const char* type_visibility_str(type_visibility_t visibility) {
    static const char* strs[] = {
        [TYPE_NOT_PUBLIC] = "private",
        [TYPE_PUBLIC] = "public",
        [TYPE_NESTED_PUBLIC] = "nested public",
        [TYPE_NESTED_PRIVATE] = "nested private",
        [TYPE_NESTED_FAMILY] = "protected",
        [TYPE_NESTED_ASSEMBLY] = "internal",
        [TYPE_NESTED_FAMILY_AND_ASSEMBLY] = "private protected",
        [TYPE_NESTED_FAMILY_OR_ASSEMBLY] = "protected internal",
    };
    return strs[visibility];
}


static bool type_is_integer(System_Type type) {
    return type == tSystem_Byte || type == tSystem_Int16 || type == tSystem_Int32 || type == tSystem_Int64 ||
           type == tSystem_SByte || type == tSystem_UInt16 || type == tSystem_UInt32 || type == tSystem_UInt64 ||
           type == tSystem_UIntPtr || type == tSystem_IntPtr || type == tSystem_Char || type == tSystem_Boolean;
}

System_Type type_get_underlying_type(System_Type T) {
    if (type_is_enum(T)) {
        return T->ElementType;
    } else {
        return T;
    }
}

static System_Type type_get_reduced_type(System_Type T) {
    T = type_get_underlying_type(T);
    if (T == tSystem_Byte) {
        return tSystem_SByte;
    } else if (T == tSystem_UInt16) {
        return tSystem_Int16;
    } else if (T == tSystem_UInt32) {
        return tSystem_Int32;
    } else if (T == tSystem_UInt64) {
        return tSystem_Int64;
    } else if (T == tSystem_UIntPtr) {
        return tSystem_IntPtr;
    } else {
        return T;
    }
}

System_Type type_get_verification_type(System_Type T) {
    T = type_get_reduced_type(T);
    if (T == tSystem_Boolean) {
        return tSystem_SByte;
    } else if (T == tSystem_Char) {
        return tSystem_Int16;
    } else if (T != NULL && T->IsByRef) {
        return get_by_ref_type(type_get_verification_type(T->BaseType));
    } else {
        return T;
    }
}

System_Type type_get_intermediate_type(System_Type T) {
    T = type_get_verification_type(T);
    if (T == tSystem_SByte || T == tSystem_Int16) {
        return tSystem_Int32;
    } else {
        return T;
    }
}

bool type_is_array_element_compatible_with(System_Type T, System_Type U) {
    System_Type V = type_get_underlying_type(T);
    System_Type W = type_get_underlying_type(U);

    if (type_is_compatible_with(V, W)) {
        return true;

    } else if (type_get_verification_type(V) == type_get_verification_type(W)) {
        // spec says it should be reduced-type, but then bool and int8 are not the same
        // and there is valid code where this happens...
        return true;
    } else {
        return false;
    }
}

bool type_is_pointer_element_compatible_with(System_Type T, System_Type U) {
    System_Type V = type_get_verification_type(T);
    System_Type W = type_get_verification_type(U);
    return V == W;
}

static System_Type type_get_direct_base_class(System_Type T) {
    if (T != NULL && T->IsArray) {
        return tSystem_Array;
    } else if (type_is_object_ref(T) || (T != NULL && type_is_interface(T))) {
        return tSystem_Object;
    } else if (T != NULL && T->IsValueType) {
        return tSystem_ValueType;
    } else {
        return NULL;
    }
}

static bool type_is_interface_directly_implemented_by(System_Type I, System_Type T) {
    if (!type_is_interface(I)) {
        return false;
    }

    if (T->InterfaceImpls == NULL) {
        return false;
    }

    for (int i = 0; i < T->InterfaceImpls->Length; i++) {
        if (T->InterfaceImpls->Data[i]->InterfaceType == I) {
            return true;
        }
    }

    return false;
}

bool type_is_compatible_with(System_Type T, System_Type U) {
    // T is identical to U.
    if (T == U) {
        return true;
    }

    // doesn't make sense to have a null type in here
    if (T == NULL || U == NULL) {
        return false;
    }

    if (type_is_object_ref(T)) {
        if (U == type_get_direct_base_class(T)) {
            return true;
        }

        if (type_is_interface_directly_implemented_by(U, T)) {
            return true;
        }
    }

    if (!T->IsValueType) {
        System_Type Base = T->BaseType;
        while (Base != NULL) {
            if (Base == U) {
                return true;
            }
            Base = Base->BaseType;
        }
    }

    if (T->IsArray && U->IsArray && type_is_array_element_compatible_with(T->ElementType, U->ElementType)) {
        return true;
    }

    if (T->IsByRef && U->IsByRef) {
        if (type_is_pointer_element_compatible_with(T, U)) {
            return true;
        }
    }

    return false;
}

static bool type_is_assignable_to(System_Type T, System_Type U) {
    if (T == U) {
        return true;
    }

    System_Type V = type_get_intermediate_type(T);
    System_Type W = type_get_intermediate_type(U);

    if (V == W) {
        return true;
    }

    // TODO: This rule seems really wtf
//    if (
//        (V == tSystem_IntPtr && W == tSystem_Int32) ||
//        (V == tSystem_Int32 && W == tSystem_IntPtr)
//    ) {
//        return true;
//    }

    if (type_is_compatible_with(T, U)) {
        return true;
    }

    if (T == NULL && type_is_object_ref(U)) {
        return true;
    }

    return false;
}

bool type_is_verifier_assignable_to(System_Type Q, System_Type R) {
    System_Type T = type_get_verification_type(Q);
    System_Type U = type_get_verification_type(R);

    if (T == U) {
        return true;
    }

    if (type_is_assignable_to(T, U)) {
        return true;
    }

    return false;
}

void type_print_name(System_Type type, strbuilder_t* builder) {
    if (type->DeclaringType != NULL) {
        type_print_name(type->DeclaringType, builder);
        strbuilder_char(builder, '+');
    } else {
        if (type->Namespace->Length > 0) {
            strbuilder_utf16(builder, type->Namespace->Chars, type->Namespace->Length);
            strbuilder_char(builder, '.');
        }
    }
    strbuilder_utf16(builder, type->Name->Chars, type->Name->Length);
}


void type_print_full_name(System_Type type, strbuilder_t* builder) {
    strbuilder_char(builder, '[');
    strbuilder_utf16(builder, type->Assembly->Name->Chars, type->Assembly->Name->Length);
    strbuilder_char(builder, '-');
    strbuilder_char(builder, 'v');
    strbuilder_uint(builder, type->Assembly->MajorVersion);
    strbuilder_char(builder, ']');
    type_print_name(type, builder);
}

void method_print_name(System_Reflection_MethodInfo method, strbuilder_t* builder) {
    strbuilder_utf16(builder, method->Name->Chars, method->Name->Length);
    strbuilder_char(builder, '(');
    for (int i = 0; i < method->Parameters->Length; i++) {
        type_print_full_name(method->Parameters->Data[i]->ParameterType, builder);
        if (i + 1 != method->Parameters->Length) {
            strbuilder_char(builder, ',');
        }
    }
    strbuilder_char(builder, ')');
}

void method_print_full_name(System_Reflection_MethodInfo method, strbuilder_t* builder) {
    type_print_full_name(method->DeclaringType, builder);
    strbuilder_char(builder, ':');
    strbuilder_char(builder, ':');
    method_print_name(method, builder);
}

System_Reflection_FieldInfo type_get_field_cstr(System_Type type, const char* name) {
    for (int i = 0; i < type->Fields->Length; i++) {
        if (string_equals_cstr(type->Fields->Data[i]->Name, name)) {
            return type->Fields->Data[i];
        }
    }
    return NULL;
}

System_Reflection_MethodInfo type_iterate_methods_cstr(System_Type type, const char* name, int* index) {
    for (int i = *index; i < type->Methods->Length; i++) {
        if (string_equals_cstr(type->Methods->Data[i]->Name, name)) {
            *index = i + 1;
            return type->Methods->Data[i];
        }
    }
    return NULL;
}

System_Reflection_MethodInfo type_get_interface_method_impl(System_Type targetType, System_Reflection_MethodInfo targetMethod) {
    Pentagon_Reflection_InterfaceImpl interface = type_get_interface_impl(targetType, targetMethod->DeclaringType);
    if (interface == NULL) {
        return NULL;
    }
    return targetType->VirtualMethods->Data[interface->VTableOffset + targetMethod->VTableOffset];
}

Pentagon_Reflection_InterfaceImpl type_get_interface_impl(System_Type targetType, System_Type interfaceType) {
    if (targetType->InterfaceImpls == NULL) {
        return NULL;
    }

    for (int i = 0; i < targetType->InterfaceImpls->Length; i++) {
        if (targetType->InterfaceImpls->Data[i]->InterfaceType == interfaceType) {
            return targetType->InterfaceImpls->Data[i];
        }
    }
    return NULL;
}

bool isinstance(System_Object object, System_Type type) {
    if (object == NULL) {
        return true;
    }
    return type_is_verifier_assignable_to(object->vtable->type, type);
}

void assembly_dump(System_Reflection_Assembly assembly) {
    strbuilder_t name = strbuilder_new();
    strbuilder_utf16(&name, assembly->Module->Name->Chars, assembly->Module->Name->Length);
    TRACE("Assembly `%s`:", strbuilder_get(&name));
    strbuilder_free(&name);
    for (int i = 0; i < assembly->DefinedTypes->Length; i++) {
        System_Type type = assembly->DefinedTypes->Data[i];

        printf("[*] \t%s %s ", type_visibility_str(type_visibility(type)), type_is_interface(type) ? "interface" : "class");
        strbuilder_t name = strbuilder_new();
        type_print_full_name(type, &name);
        if (type->BaseType != NULL) {
            strbuilder_cstr(&name, " : ");
            type_print_full_name(type->BaseType, &name);
        }
        printf("%s\r\n", strbuilder_get(&name));
        strbuilder_free(&name);

        for (int j = 0; j < type->Fields->Length; j++) {
            strbuilder_t field = strbuilder_new();
            strbuilder_cstr(&field, field_access_str(field_access(type->Fields->Data[j])));
            strbuilder_char(&field, ' ');
            strbuilder_cstr(&field, field_is_static(type->Fields->Data[j]) ? "static " : "");
            
            strbuilder_utf16(&field, type->Fields->Data[j]->FieldType->Namespace->Chars, type->Fields->Data[j]->FieldType->Namespace->Length);
            strbuilder_char(&field, '.');
            strbuilder_utf16(&field, type->Fields->Data[j]->FieldType->Name->Chars, type->Fields->Data[j]->FieldType->Name->Length);
            strbuilder_char(&field, ' ');
            
            strbuilder_utf16(&field, type->Fields->Data[j]->Name->Chars, type->Fields->Data[j]->Name->Length);
            
            TRACE("\t\t%s; // offset 0x%02x", strbuilder_get(&field), type->Fields->Data[j]->MemoryOffset);
            strbuilder_free(&field);
        }

        for (int j = 0; j < type->Methods->Length; j++) {
            System_Reflection_MethodInfo mi =  type->Methods->Data[j];

            printf("[*] \t\t");

            strbuilder_t method = strbuilder_new();

            if (method_is_static(mi)) {
                strbuilder_cstr(&method, "static ");
            }

            if (method_is_abstract(mi)) {
                strbuilder_cstr(&method, "abstract ");
            }

            if (method_is_final(mi)) {
                strbuilder_cstr(&method, "final ");
            }

            if (method_is_virtual(mi)) {
                strbuilder_cstr(&method, "virtual[");
                strbuilder_uint(&method, mi->VTableOffset);
                strbuilder_cstr(&method, "] ");
            }

            if (mi->ReturnType == NULL) {
                strbuilder_cstr(&method, "void");
            } else {
                type_print_full_name(mi->ReturnType, &method);
            }
            strbuilder_char(&method, ' ');
            method_print_full_name(mi, &method);
            printf("%s\r\n", strbuilder_get(&method));
            strbuilder_free(&method);
            
            if (
                method_get_code_type(mi) == METHOD_IL &&
                !method_is_unmanaged(mi) &&
                !method_is_abstract(mi) &&
                !method_is_internal_call(mi)
            ) {
                // handle locals
                for (int li = 0; li < mi->MethodBody->LocalVariables->Length; li++) {
                    printf("[*] \t\t\t");
                    strbuilder_t local = strbuilder_new();
                    type_print_full_name(mi->MethodBody->LocalVariables->Data[li]->LocalType, &local);
                    strbuilder_cstr(&local, " V_");
                    strbuilder_uint(&local, mi->MethodBody->LocalVariables->Data[li]->LocalIndex);
                    printf("%s\r\n", strbuilder_get(&local));
                    strbuilder_free(&local);
                }

                opcode_disasm_method(mi);
            } else if (method_get_code_type(mi) == METHOD_NATIVE) {
                TRACE("\t\t\t<native method>");
            } else if (method_get_code_type(mi) == METHOD_RUNTIME) {
                TRACE("\t\t\t<runtime method>");
            }
        }

        TRACE("");
    }
}
