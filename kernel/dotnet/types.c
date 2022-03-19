#include <string.h>
#include "types.h"

#include "gc.h"
#include "loader.h"

System_String* new_string_from_cstr(const char* str) {
    int len = (int)strlen(str);

    System_String* string = gc_new(typeof_System_String, sizeof(System_String) + sizeof(wchar_t) * len);

    string->Length = len;
    for (int i = 0; i < len; i++) {
        string->Chars[i] = (wchar_t)str[i];
    }

    return string;
}

System_Type* get_array_type(System_Type* baseType) {
    if (baseType == NULL) {
        return NULL;
    }

    // fast path, there is alreayd an array type for this
    if (baseType->ArrayType != NULL) {
        return baseType->ArrayType;
    }

    // slow path, we need a new type, lock mutex and
    // check if we already happen to have it, if not
    // allocate one
    spinlock_lock(&baseType->TypeLock);
    if (baseType->ArrayType != NULL) {
        spinlock_unlock(&baseType->TypeLock);
        return baseType->ArrayType;
    }

    // allocate a new type object for the new array type
    System_Type* new_array_type = gc_new(NULL, sizeof(System_Type));
    gc_update((System_Object*)baseType, offsetof(System_Type, ArrayType), (System_Object*)new_array_type);

    // setup the new obejct
    new_array_type->Name = typeof_System_Array->Name;
    new_array_type->Namespace = typeof_System_Array->Namespace;
    new_array_type->Assembly = typeof_System_Array->Assembly;
    new_array_type->ElementType = baseType;

    // we can safely unlock the mutex
    spinlock_unlock(&baseType->TypeLock);

    return baseType->ArrayType;
}
