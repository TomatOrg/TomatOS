#include "types.h"

#include <runtime/dotnet/gc/gc.h>

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
    System_Type ArrayType = gc_new(tSystem_Array, tSystem_Array->managed_size);

    // copy the System.Array base information
    GC_UPDATE(Type->ArrayType, MemberInfo.Module, tSystem_Array->MemberInfo.Module);
    GC_UPDATE(Type->ArrayType, MemberInfo.Name, tSystem_Array->MemberInfo.Name);
    GC_UPDATE(Type->ArrayType, Assembly, tSystem_Array->Assembly);
    GC_UPDATE(Type->ArrayType, FullName, tSystem_Array->FullName);
    GC_UPDATE(Type->ArrayType, Module, tSystem_Array->Module);
    GC_UPDATE(Type->ArrayType, Namespace, tSystem_Array->Namespace);
    // TODO: copy more stuff

    Type->ArrayType->stack_size = tSystem_Array->stack_size;
    Type->ArrayType->managed_size = tSystem_Array->managed_size;

    // Set the ElementType
    GC_UPDATE(Type->ArrayType, ElementType, Type);

    // Set the array type
    GC_UPDATE(Type->ArrayType, ArrayType, ArrayType);
    mutex_unlock(&Type->ArrayTypeMutex);

    return Type->ArrayType;
}
