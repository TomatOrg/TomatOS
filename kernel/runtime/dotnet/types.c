#include "types.h"

#include <runtime/dotnet/gc/gc.h>

System_Type tSystem_Type = NULL;
System_Type tSystem_Array = NULL;
System_Type tSystem_String = NULL;
System_Type tSystem_Reflection_Module = NULL;
System_Type tSystem_Reflection_Assembly = NULL;
System_Type tSystem_Reflection_FieldInfo = NULL;

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

    mutex_lock(&Type->ArrayTypeMutex);

    if (Type->ArrayType != NULL) {
        mutex_unlock(&Type->ArrayTypeMutex);
        return Type->ArrayType;
    }

    // allocate the new type
    System_Type ArrayType = GC_NEW(tSystem_Array);

    // copy the System.Array base information
    GC_UPDATE(ArrayType, Module, tSystem_Array->Module);
    GC_UPDATE(ArrayType, Name, tSystem_Array->Name);
    GC_UPDATE(ArrayType, Assembly, tSystem_Array->Assembly);
    GC_UPDATE(ArrayType, FullName, tSystem_Array->FullName);
    GC_UPDATE(ArrayType, Module, tSystem_Array->Module);
    GC_UPDATE(ArrayType, Namespace, tSystem_Array->Namespace);
    // TODO: copy more stuff

    ArrayType->stack_size = tSystem_Array->stack_size;
    ArrayType->managed_size = tSystem_Array->managed_size;

    // Set the ElementType
    GC_UPDATE(ArrayType, ElementType, Type);

    // Set the array type
    GC_UPDATE(Type, ArrayType, ArrayType);
    mutex_unlock(&Type->ArrayTypeMutex);

    return Type->ArrayType;
}
