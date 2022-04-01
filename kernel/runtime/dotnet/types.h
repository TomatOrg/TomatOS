#pragma once

#include <sync/mutex.h>

#include <stdint.h>
#include <stddef.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct System_Object *System_Object;
typedef struct System_Type *System_Type;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Represents a dotnet object
 */
struct System_Object {
    // the type of the object, must be first
    System_Type type;

    // the log pointer, for tracing object changes
    System_Object* log_pointer;

    // the color of the object, black and white switch during collection
    // and blue means unallocated
    uint8_t color;

    // the rank of the object from the allocator
    uint8_t rank;

    uint8_t _reserved0;
    uint8_t _reserved1;

    // next free object in the chunk
    System_Object next;

    // next chunk
    System_Object chunk_next;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct System_Array {
    struct System_Object Object;
    int Length;
} *System_Array;

#define DEFINE_ARRAY(type) \
    typedef struct type##_Array { \
        struct System_Array Array; \
        type* Data;\
    } type##_Array;

DEFINE_ARRAY(System_Type);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct System_String {
    struct System_Object Object;
    int Length;
    wchar_t Chars[];
} *System_String;

DEFINE_ARRAY(System_String);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct System_Reflection_Module *System_Reflection_Module;
typedef struct System_Reflection_Assembly *System_Reflection_Assembly;

DEFINE_ARRAY(System_Reflection_Module);
DEFINE_ARRAY(System_Reflection_Assembly);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct System_Reflection_Assembly {
    struct System_Object Object;
    System_Type_Array DefinedTypes;
    System_Type_Array ExportedTypes;
    System_Reflection_Module_Array Modules;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct System_Reflection_Module {
    struct System_Object Object;
    System_Reflection_Assembly Assembly;
    int MetadataToken;
    System_String Name;
    System_String ScopeName;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct System_Reflection_MemberInfo {
    struct System_Object Object;
    System_Reflection_Module Module;
    System_String Name;
} *System_Reflection_MemberInfo;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct System_Reflection_FieldInfo {
    struct System_Reflection_MemberInfo MemberInfo;
    System_Type FieldType;
} *System_Reflection_FieldInfo;

DEFINE_ARRAY(System_Reflection_FieldInfo);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct System_Type {
    struct System_Reflection_MemberInfo MemberInfo;
    System_Reflection_Assembly Assembly;
    System_Type BaseType;
    System_Type DeclaringType;
    System_String FullName;
    System_Reflection_Module Module;
    System_String Namespace;
    System_Reflection_FieldInfo_Array Fields;
    System_Type ElementType;

    //
    // For the runtime, unrelated to the System.Type stuff
    //

    mutex_t ArrayTypeMutex;
    System_Type ArrayType;

    size_t stack_size;
    size_t managed_size;
};

/**
 * Get the array type for the given type
 *
 * @param Type  [IN] The system type
 */
System_Type get_array_type(System_Type Type);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern System_Type tSystem_Type;
extern System_Type tSystem_Array;
extern System_Type tSystem_String;

/**
 * Get all the builtin types from the core library
 */
err_t init_dotnet_types();
