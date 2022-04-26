#include "types.h"
#include "util/stb_ds.h"

#include <runtime/dotnet/gc/gc.h>

#include <util/string.h>

#include <mem/mem.h>

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
System_Type tSystem_Reflection_ParameterInfo = NULL;
System_Type tSystem_Reflection_LocalVariableInfo = NULL;
System_Type tSystem_Reflection_ExceptionHandlingClause = NULL;
System_Type tSystem_Reflection_MethodBase = NULL;
System_Type tSystem_Reflection_MethodBody = NULL;
System_Type tSystem_Reflection_MethodInfo = NULL;

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
    memcpy(new->Chars, old->Chars, sizeof(wchar_t) * old->Length);

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
            ASSERT(!"assembly_get_type_by_token: TODO: TypeRef");
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

        default:
            ASSERT(!"assembly_get_field_by_token: invalid table for type");
            return NULL;
    }
}

System_Type get_array_type(System_Type Type) {
    if (Type->ArrayType != NULL) {
        return Type->ArrayType;
    }

    mutex_lock(&Type->ArrayTypeMutex);

    if (Type->ArrayType != NULL) {
        mutex_unlock(&Type->ArrayTypeMutex);
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

    // set the sizes properly
    ArrayType->StackSize = tSystem_Array->StackSize;
    ArrayType->ManagedSize = tSystem_Array->ManagedSize;
    ArrayType->StackAlignment = tSystem_Array->StackAlignment;
    ArrayType->ManagedAlignment = tSystem_Array->ManagedAlignment;

    // There are no managed pointers in here (The gc will handle array
    // stuff on its own)
    ArrayType->ManagedPointersOffsets = NULL;

    // Set the element type
    GC_UPDATE(ArrayType, ElementType, Type);

    // Set the array type
    GC_UPDATE(Type, ArrayType, ArrayType);
    mutex_unlock(&Type->ArrayTypeMutex);

    return Type->ArrayType;
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
    }
    // TODO: managed pointer
    else {
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
    } else if (type_get_reduced_type(V) == type_get_reduced_type(W)) {
        return true;
    } else {
        return false;
    }
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

    return false;
}

static bool type_is_pointer_element_compatible_with(System_Type T, System_Type U) {
    System_Type V = type_get_verification_type(T);
    System_Type W = type_get_verification_type(U);
    return V == W;
}

static bool type_is_location_compatible_with(System_Type T, System_Type U) {
    if (T->IsValueType && U->IsValueType && type_is_compatible_with(T, U)) {
        return true;
    }

    if (!T->IsValueType && !U->IsValueType && type_is_pointer_element_compatible_with(T, U)) {
        return true;
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
