#pragma once

#include <sync/mutex.h>

#include <stdint.h>
#include <stddef.h>
#include "runtime/dotnet/metadata/metadata_spec.h"
#include "runtime/dotnet/metadata/metadata.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct System_Object *System_Object;
typedef struct System_Type *System_Type;
typedef struct System_Reflection_MethodInfo *System_Reflection_MethodInfo;
typedef struct System_Reflection_FieldInfo *System_Reflection_FieldInfo;

typedef struct System_Guid {
    uint32_t a;
    uint16_t b;
    uint16_t c;
    uint8_t d;
    uint8_t e;
    uint8_t f;
    uint8_t g;
    uint8_t h;
    uint8_t i;
    uint8_t j;
    uint8_t k;
} System_Guid;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct System_ValueType {
    // empty...
} System_ValueType;

typedef bool System_Boolean;
typedef wchar_t System_Char;
typedef int8_t System_SByte;
typedef uint8_t System_Byte;
typedef int16_t System_Int16;
typedef uint16_t System_UInt16;
typedef int32_t System_Int32;
typedef uint32_t System_UInt32;
typedef int64_t System_Int64;
typedef uint64_t System_UInt64;
typedef float System_Single;
typedef double System_Double;
typedef intptr_t System_IntPtr;
typedef uintptr_t System_UIntPtr;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Represents a dotnet object
 */
struct System_Object {
    union {
        // while the object is alive
        struct {
            // the type of the object, must be first
            System_Type type;

            // the log pointer, for tracing object changes
            System_Object* log_pointer;

            // the color of the object, black and white switch during collection
            // and blue means unallocated
            uint8_t color;

            // the rank of the object from the allocator
            uint8_t rank;

            // if true the finalizer should not run
            uint8_t suppress_finalizer;

            // the app domain this object was created under
            uint8_t _reserved0;
            uint8_t _reserved1;
            uint8_t _reserved2;
            uint8_t _reserved3;
            uint8_t _reserved4;
        };

        // while the object is in the heap
        struct {
            // next chunk
            System_Object chunk_next;
        };
    };

    // next free object in the chunk, and the next
    // object in general on the heap for sweeping
    System_Object next;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct System_Array {
    struct System_Object;
    int Length;
} *System_Array;

#define DEFINE_ARRAY(type) \
    typedef struct type##_Array { \
        struct System_Array; \
        type Data[0];\
    } *type##_Array;

DEFINE_ARRAY(System_Type);
DEFINE_ARRAY(System_Reflection_MethodInfo);
DEFINE_ARRAY(System_Byte);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct System_String {
    struct System_Object;
    int Length;
    wchar_t Chars[];
} *System_String;

DEFINE_ARRAY(System_String);

bool string_equals_cstr(System_String a, const char* b);

bool string_equals(System_String a, System_String b);

/**
 * Append a c null terminated ascii string to the given string, this
 * creates a new copy of the string
 *
 * @param old   [IN] The old string to append to
 * @param str   [IN] The string to append
 */
System_String string_append_cstr(System_String old, const char* str);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct System_Reflection_Module *System_Reflection_Module;
typedef struct System_Reflection_Assembly *System_Reflection_Assembly;

DEFINE_ARRAY(System_Reflection_Module);
DEFINE_ARRAY(System_Reflection_Assembly);
DEFINE_ARRAY(System_Reflection_FieldInfo);
DEFINE_ARRAY(System_Int32);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct System_Reflection_Assembly {
    struct System_Object;
    System_Type_Array DefinedTypes;
    System_Reflection_MethodInfo_Array DefinedMethods;
    System_Reflection_FieldInfo_Array DefinedFields;
    System_Reflection_Module Module;
};

/**
 * Get a type by its token, returns NULL if not found
 *
 * @param assembly  [IN] The assembly this token is coming from
 * @param token     [IN] The token of the type to get
 */
System_Type assembly_get_type_by_token(System_Reflection_Assembly assembly, token_t token);

System_Reflection_MethodInfo assembly_get_method_by_token(System_Reflection_Assembly assembly, token_t token);

System_Reflection_FieldInfo assembly_get_field_by_token(System_Reflection_Assembly assembly, token_t token);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct System_Reflection_Module {
    struct System_Object;
    System_Reflection_Assembly Assembly;
    System_String Name;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct System_Reflection_MemberInfo {
    struct System_Object;
    System_Type DeclaringType;
    System_Reflection_Module Module;
    System_String Name;
} *System_Reflection_MemberInfo;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct System_Reflection_FieldInfo {
    struct System_Reflection_MemberInfo;
    System_Type FieldType;
    uintptr_t MemoryOffset; // can be either absolute offset to a literal/rva, or an offset from
                            // the start, depending on the attributes of the field
    uint16_t Attributes;
};

typedef enum field_access {
    FIELD_COMPILER_CONTROLLED,
    FIELD_PRIVATE,
    FIELD_FAMILY_AND_ASSEMBLY,
    FIELD_ASSEMBLY,
    FIELD_FAMILY,
    FIELD_FAMILY_OR_ASSEMBLY,
    FIELD_PUBLIC,
} field_access_t;

static inline field_access_t field_access(System_Reflection_FieldInfo field) { return field->Attributes & 0b111; }
static inline bool field_is_static(System_Reflection_FieldInfo field) { return field->Attributes & 0x0010; }
static inline bool field_is_init_only(System_Reflection_FieldInfo field) { return field->Attributes & 0x0020; }
static inline bool field_is_literal(System_Reflection_FieldInfo field) { return field->Attributes & 0x0040; }

const char* field_access_str(field_access_t access);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct System_Reflection_ParameterInfo {
    struct System_Object;
    uint16_t Attributes;
    System_String Name;
    System_Type ParameterType;
} *System_Reflection_ParameterInfo;

DEFINE_ARRAY(System_Reflection_ParameterInfo);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct System_Reflection_MethodBody {
    struct System_Object;
    int32_t MaxStackSize;
    System_Byte_Array Il;
} *System_Reflection_MethodBody;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct System_Reflection_MethodBase {
    struct System_Reflection_MemberInfo;
    uint16_t ImplAttributes;
    uint16_t Attributes;
    System_Reflection_MethodBody MethodBody;
    System_Reflection_ParameterInfo_Array Parameters;
} *System_Reflection_MethodBase;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct System_Reflection_MethodInfo {
    struct System_Reflection_MethodBase;
    System_Type ReturnType;

    bool IsFilled;
    int VtableOffset;
};

typedef enum method_access {
    METHOD_COMPILER_CONTROLLED = 0x0000,
    METHOD_PRIVATE = 0x0001,
    METHOD_FAMILY_AND_ASSEMBLY = 0x0002,
    METHOD_ASSEMBLY = 0x0003,
    METHOD_FAMILY = 0x0004,
    METHOD_FAMILY_OR_ASSEMBLY = 0x0005,
    METHOD_PUBLIC = 0x0006
} method_access_t;

static inline method_access_t method_get_access(System_Reflection_MethodInfo method) { return (method_access_t)(method->Attributes & 0x0007); }

// method attribute impl helpers

typedef enum method_code_type {
    METHOD_IL = 0x0000,
    METHOD_NATIVE = 0x0001,
    METHOD_RUNTIME = 0x0003,
} method_code_type_t;

static inline method_code_type_t method_get_code_type(System_Reflection_MethodInfo method) { return (method_code_type_t)(method->ImplAttributes & 0x0003); }
static inline bool method_is_unmanaged(System_Reflection_MethodInfo method) { return method->ImplAttributes & 0x0004; }
static inline bool method_is_forward_ref(System_Reflection_MethodInfo method) { return method->ImplAttributes & 0x0010; }
static inline bool method_is_preserve_sig(System_Reflection_MethodInfo method) { return method->ImplAttributes & 0x0080; }
static inline bool method_is_internal_call(System_Reflection_MethodInfo method) { return method->ImplAttributes & 0x1000; }
static inline bool method_is_synchronized(System_Reflection_MethodInfo method) { return method->ImplAttributes & 0x0020; }
static inline bool method_is_no_inlining(System_Reflection_MethodInfo method) { return method->ImplAttributes & 0x0008; }
static inline bool method_is_no_optimization(System_Reflection_MethodInfo method) { return method->ImplAttributes & 0x0040; }

// method attributes helpers
static inline bool method_is_static(System_Reflection_MethodInfo method) { return method->Attributes & 0x0010; }
static inline bool method_is_final(System_Reflection_MethodInfo method) { return method->Attributes & 0x0020; }
static inline bool method_is_virtual(System_Reflection_MethodInfo method) { return method->Attributes & 0x0040; }
static inline bool method_is_hide_by_sig(System_Reflection_MethodInfo method) { return method->Attributes & 0x0080; }
static inline bool method_is_new_slot(System_Reflection_MethodInfo method) { return method->Attributes & 0x0100; }
static inline bool method_is_strict(System_Reflection_MethodInfo method) { return method->Attributes & 0x0200; }
static inline bool method_is_abstract(System_Reflection_MethodInfo method) { return method->Attributes & 0x0400; }
static inline bool method_is_special_name(System_Reflection_MethodInfo method) { return method->Attributes & 0x0800; }
static inline bool method_is_pinvoke_impl(System_Reflection_MethodInfo method) { return method->Attributes & 0x2000; }
static inline bool method_is_rt_special_name(System_Reflection_MethodInfo method) { return method->Attributes & 0x1000; }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct System_Exception *System_Exception;

struct System_Exception {
    struct System_Object;
    System_String Message;
    System_Exception InnerException;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct System_Type {
    struct System_Reflection_MemberInfo;
    System_Reflection_Assembly Assembly;
    System_Type BaseType;
    System_String Namespace;
    System_Reflection_FieldInfo_Array Fields;
    System_Reflection_MethodInfo_Array Methods;
    System_Type ElementType;
    uint32_t Attributes;
    bool IsArray;
    bool IsByRef;
    bool IsPointer;
    System_Type_Array GenericTypeArguments;
    System_Type_Array GenericTypeParameters;
    System_Type GenericTypeDefinition;

    int* ManagedPointersOffsets;
    bool IsFilled;
    bool IsValueType;
    System_Reflection_MethodInfo_Array VirtualMethods;
    System_Reflection_MethodInfo Finalize;
    int ManagedSize;
    int ManagedAlignment;
    int StackSize;
    int StackAlignment;

    System_Type ArrayType;
    mutex_t ArrayTypeMutex;
};

static inline bool type_is_generic_definition(System_Type type) { return type->GenericTypeParameters != NULL; }
static inline bool type_is_generic_type(System_Type type) { return type_is_generic_definition(type) || type->GenericTypeArguments != NULL; }

typedef enum type_visibility {
    TYPE_NOT_PUBLIC,
    TYPE_PUBLIC,
    TYPE_NESTED_PUBLIC,
    TYPE_NESTED_PRIVATE,
    TYPE_NESTED_FAMILY,
    TYPE_NESTED_ASSEMBLY,
    TYPE_NESTED_FAMILY_AND_ASSEMBLY,
    TYPE_NESTED_FAMILY_OR_ASSEMBLY,
} type_visibility_t;

typedef enum type_layout {
    TYPE_AUTO_LAYOUT = 0,
    TYPE_SEQUENTIAL_LAYOUT = 1,
    TYPE_EXPLICIT_LAYOUT = 2,
} type_layout_t;

static inline type_visibility_t type_visibility(System_Type Type) { return Type->Attributes & 0b111; }
static inline type_layout_t type_layout(System_Type Type) { return (Type->Attributes >> 3) & 0b11; }
static inline bool type_is_abstract(System_Type Type) { return Type->Attributes & 0x00000080; }
static inline bool type_is_sealed(System_Type Type) { return Type->Attributes & 0x00000100; }

const char* type_visibility_str(type_visibility_t visibility);

/**
 * Get the array type for the given type
 *
 * @param Type  [IN] The system type
 */
System_Type get_array_type(System_Type Type);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern System_Type tSystem_AppDomain;
extern System_Type tSystem_Exception;
extern System_Type tSystem_ValueType;
extern System_Type tSystem_Object;
extern System_Type tSystem_Type;
extern System_Type tSystem_Array;
extern System_Type tSystem_String;
extern System_Type tSystem_Boolean;
extern System_Type tSystem_Char;
extern System_Type tSystem_SByte;
extern System_Type tSystem_Byte;
extern System_Type tSystem_Int16;
extern System_Type tSystem_UInt16;
extern System_Type tSystem_Int32;
extern System_Type tSystem_UInt32;
extern System_Type tSystem_Int64;
extern System_Type tSystem_UInt64;
extern System_Type tSystem_Single;
extern System_Type tSystem_Double;
extern System_Type tSystem_IntPtr;
extern System_Type tSystem_UIntPtr;
extern System_Type tSystem_Reflection_Module;
extern System_Type tSystem_Reflection_Assembly;
extern System_Type tSystem_Reflection_FieldInfo;
extern System_Type tSystem_Reflection_ParameterInfo;
extern System_Type tSystem_Reflection_MethodBase;
extern System_Type tSystem_Reflection_MethodBody;
extern System_Type tSystem_Reflection_MethodInfo;
