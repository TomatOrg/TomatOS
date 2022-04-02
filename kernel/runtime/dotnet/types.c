#include "types.h"
#include "util/stb_ds.h"

#include <runtime/dotnet/gc/gc.h>

#include <util/string.h>

#include <mem/mem.h>

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
System_Type tSystem_Reflection_MethodBase = NULL;
System_Type tSystem_Reflection_MethodBody = NULL;
System_Type tSystem_Reflection_MethodInfo = NULL;

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

System_Type get_type_by_token(System_Reflection_Assembly assembly, token_t token) {
    if (token.index == 0) {
        ASSERT(!"get_type_by_token: NULL token");
        return NULL;
    }

    switch (token.table) {
        case METADATA_TYPE_DEF: {
            if (token.index - 1 >= assembly->DefinedTypes->Length) {
                ASSERT(!"get_type_by_token: token outside of range");
                return NULL;
            }
            return assembly->DefinedTypes->Data[token.index - 1];
        } break;

        default:
            ASSERT(!"get_type_by_token: invalid table for type");
            return NULL;
    }
}

System_Type get_array_type(System_Type Type) {
    if (Type->ArrayType != NULL) {
        return Type->ArrayType;
    }

    mutex_lock(Type->ArrayTypeMutex);

    if (Type->ArrayType != NULL) {
        mutex_unlock(Type->ArrayTypeMutex);
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

    // set the sizes properly
    ArrayType->StackSize = tSystem_Array->StackSize;
    ArrayType->ManagedSize = tSystem_Array->ManagedSize;
    ArrayType->StackAlignment = tSystem_Array->StackAlignment;
    ArrayType->ManagedAlignment = tSystem_Array->ManagedAlignment;

    // set the type mutex
    ArrayType->ArrayTypeMutex = malloc(sizeof(mutex_t));

    // There are no managed pointers in here (The gc will handle array
    // stuff on its own)
    ArrayType->ManagedPointersOffsets = NULL;

    // Set the element type
    GC_UPDATE(ArrayType, ElementType, Type);

    // Set the array type
    GC_UPDATE(Type, ArrayType, ArrayType);
    mutex_unlock(Type->ArrayTypeMutex);

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
