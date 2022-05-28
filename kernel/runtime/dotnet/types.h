#pragma once

#include "runtime/dotnet/metadata/metadata_spec.h"
#include "runtime/dotnet/metadata/metadata.h"

#include <mir/mir.h>

#include <sync/mutex.h>

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

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

typedef struct System_Enum {
    // empty...
} System_Enum;

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

typedef struct object_vtable {
    System_Type type;
    void* virtual_functions[0];
} object_vtable_t;

/**
 * Represents a dotnet object
 */
struct System_Object {
    // the type of the object
    object_vtable_t* vtable;

    // the color of the object
    uint8_t color : 3;
#define COLOR_BLUE      0   /* unallocated object */
#define COLOR_WHITE     1   /* object that has not been traced */
#define COLOR_GRAY      2   /* object that has been traced, but its children have not been traced yet */
#define COLOR_BLACK     3   /* object that has been traced, and its children have been traced as well */
#define COLOR_YELLOW    4   /* object that has not been traced (for color switching) */
#define COLOR_GREEN     5   /* object that should be finalized */
#define COLOR_RESERVED0 6   /* reserved for future use */
#define COLOR_RESERVED1 7   /* reserved for future use */

    // should finalizer be called or not
    uint8_t suppress_finalizer : 1;

    uint8_t _reserved0 : 4;

    uint8_t _reserved1;
    uint8_t _reserved2;
    uint8_t _reserved3;
    uint32_t _reserved4;
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
typedef struct System_Reflection_MemberInfo *System_Reflection_MemberInfo;

DEFINE_ARRAY(System_Reflection_Module);
DEFINE_ARRAY(System_Reflection_Assembly);
DEFINE_ARRAY(System_Reflection_FieldInfo);
DEFINE_ARRAY(System_Reflection_MemberInfo);
DEFINE_ARRAY(System_Int32);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct System_Reflection_Assembly {
    struct System_Object;

    // the module and entry point of this assembly
    System_Reflection_Module Module;
    System_Reflection_MethodInfo EntryPoint;

    // types defined inside the binary
    System_Type_Array DefinedTypes;
    System_Reflection_MethodInfo_Array DefinedMethods;
    System_Reflection_FieldInfo_Array DefinedFields;

    // types imported from other assemblies, for easy lookup whenever needed
    System_Type_Array ImportedTypes;
    System_Reflection_MemberInfo_Array ImportedMembers;

    // we have two entries, one for GC tracking (the array)
    // and one for internally looking up the string entries
    // TODO: turn into a Dictionary for easy management
    System_String_Array UserStrings;
    struct {
        int key;
        System_String value;
    }* UserStringsTable;
};

System_Type assembly_get_type_by_token(System_Reflection_Assembly assembly, token_t token);
System_Reflection_MethodInfo assembly_get_method_by_token(System_Reflection_Assembly assembly, token_t token);
System_Reflection_FieldInfo assembly_get_field_by_token(System_Reflection_Assembly assembly, token_t token);

System_Type assembly_get_type_by_name(System_Reflection_Assembly assembly, const char* name, const char* namespace);

System_String assembly_get_string_by_token(System_Reflection_Assembly assembly, token_t token);

void assembly_dump(System_Reflection_Assembly assembly);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct System_Reflection_Module {
    struct System_Object;
    System_Reflection_Assembly Assembly;
    System_String Name;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct System_Reflection_MemberInfo {
    struct System_Object;
    System_Type DeclaringType;
    System_Reflection_Module Module;
    System_String Name;
};

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

typedef struct System_Reflection_LocalVariableInfo {
    struct System_Object;
    int LocalIndex;
    System_Type LocalType;
} *System_Reflection_LocalVariableInfo;

DEFINE_ARRAY(System_Reflection_LocalVariableInfo);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef enum System_Reflection_ExceptionHandlingClauseOptions {
    ExceptionHandlingClauseOptions_Clause = 0,
    ExceptionHandlingClauseOptions_Fault = 4,
    ExceptionHandlingClauseOptions_Filter = 1,
    ExceptionHandlingClauseOptions_Finally = 2,
} System_Reflection_ExceptionHandlingClauseOptions;

typedef struct System_Reflection_ExceptionHandlingClause {
    struct System_Object;
    System_Type CatchType;
    int FilterOffset;
    System_Reflection_ExceptionHandlingClauseOptions Flags;
    int HandlerLength;
    int HandlerOffset;
    int TryLength;
    int TryOffset;
} *System_Reflection_ExceptionHandlingClause;

DEFINE_ARRAY(System_Reflection_ExceptionHandlingClause);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct System_Reflection_MethodBody {
    struct System_Object;
    System_Reflection_ExceptionHandlingClause_Array ExceptionHandlingClauses;
    System_Reflection_LocalVariableInfo_Array LocalVariables;
    bool InitLocals;
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
    int VTableOffset;

    MIR_item_t MirFunc;
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

/**
 * Print the method name as<name>(<parameters>) <name>(<parameters>)
 */
void method_print_name(System_Reflection_MethodInfo method, FILE* output);

/**
 * Print the full method name as [<assembly>]<namespace>.<class>[+<nested>]::<name>(<parameters>)
 */
void method_print_full_name(System_Reflection_MethodInfo method, FILE* output);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct System_Exception *System_Exception;

struct System_Exception {
    struct System_Object;
    System_String Message;
    System_Exception InnerException;
};

typedef System_Exception System_ArithmeticException;
typedef System_Exception System_DivideByZeroException;
typedef System_Exception System_ExecutionEngineException;
typedef System_Exception System_IndexOutOfRangeException;
typedef System_Exception System_NullReferenceException;
typedef System_Exception System_OutOfMemoryException;
typedef System_Exception System_OverflowException;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct Pentagon_Reflection_InterfaceImpl {
    struct System_Object;
    System_Type InterfaceType;
    int VTableOffset;
} *Pentagon_Reflection_InterfaceImpl;
DEFINE_ARRAY(Pentagon_Reflection_InterfaceImpl);

// TODO: should we maybe have this more customized for our needs
//       so for example differentiate Object and interface, and have
//       two float types (32 and 64)
typedef enum stack_type {
    STACK_TYPE_O,
    STACK_TYPE_INT32,
    STACK_TYPE_INT64,
    STACK_TYPE_INTPTR,
    STACK_TYPE_VALUE_TYPE,
    STACK_TYPE_FLOAT,
    STACK_TYPE_REF,
} stack_type_t;

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
    object_vtable_t* VTable;
    stack_type_t StackType;

    Pentagon_Reflection_InterfaceImpl_Array InterfaceImpls;

    System_Type ArrayType;
    System_Type ByRefType;
    mutex_t TypeMutex;
};

static inline stack_type_t type_get_stack_type(System_Type type) { return type == NULL ? STACK_TYPE_O : type->StackType; }

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

static inline type_visibility_t type_visibility(System_Type type) { return type->Attributes & 0b111; }
static inline type_layout_t type_layout(System_Type type) { return (type->Attributes >> 3) & 0b11; }
static inline bool type_is_abstract(System_Type type) { return type->Attributes & 0x00000080; }
static inline bool type_is_sealed(System_Type type) { return type->Attributes & 0x00000100; }
static inline bool type_is_interface(System_Type type) { return type != NULL && type->Attributes & 0x00000020; }

const char* type_visibility_str(type_visibility_t visibility);

/**
 * Get the array type for the given type
 *
 * @param Type  [IN] The type
 */
System_Type get_array_type(System_Type Type);

/**
 * Get the by-ref type for the given type
 *
 * @param Type  [IN] The type
 */
System_Type get_by_ref_type(System_Type Type);

/**
 * Print the type name as <namespace>.<class>[+<nested>]
 */
void type_print_name(System_Type Type, FILE* output);

/**
 * Print the full name as [assembly]<namespace>.<class>[+<nested>]
 */
void type_print_full_name(System_Type Type, FILE* output);

/**
 * Get a field by its name
 *
 * TODO: take into account member types
 *
 * @param type      [IN] The declaring type
 * @param name      [IN] The name
 */
System_Reflection_FieldInfo type_get_field_cstr(System_Type type, const char* name);

/**
 * Iterate all the methods of the type with the same name
 *
 * TODO: take into account member types
 *
 * @param type      [IN] The declaring type
 * @param name      [IN] The name of the type
 * @param index     [IN] The index from which to continue
 */
System_Reflection_MethodInfo type_iterate_methods_cstr(System_Type type, const char* name, int* index);

/**
 * Get the implementation of the given interface method
 */
System_Reflection_MethodInfo type_get_interface_method_impl(System_Type targetType, System_Reflection_MethodInfo targetMethod);

/**
 * Get the interface implementation of the given type
 */
Pentagon_Reflection_InterfaceImpl type_get_interface_impl(System_Type targetType, System_Type interfaceType);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern System_Type tSystem_Enum;
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
extern System_Type tSystem_Delegate;
extern System_Type tSystem_MulticastDelegate;
extern System_Type tSystem_Reflection_Module;
extern System_Type tSystem_Reflection_Assembly;
extern System_Type tSystem_Reflection_FieldInfo;
extern System_Type tSystem_Reflection_MemberInfo;
extern System_Type tSystem_Reflection_ParameterInfo;
extern System_Type tSystem_Reflection_LocalVariableInfo;
extern System_Type tSystem_Reflection_ExceptionHandlingClause;
extern System_Type tSystem_Reflection_MethodBase;
extern System_Type tSystem_Reflection_MethodBody;
extern System_Type tSystem_Reflection_MethodInfo;
extern System_Type tSystem_ArithmeticException;
extern System_Type tSystem_DivideByZeroException;
extern System_Type tSystem_ExecutionEngineException;
extern System_Type tSystem_IndexOutOfRangeException;
extern System_Type tSystem_NullReferenceException;
extern System_Type tSystem_OutOfMemoryException;
extern System_Type tSystem_OverflowException;

extern System_Type tPentagon_Reflection_InterfaceImpl;

static inline bool type_is_enum(System_Type type) { return type != NULL && type->BaseType == tSystem_Enum; }
static inline bool type_is_object_ref(System_Type type) { return type == NULL || !type->IsValueType; }
static inline bool type_is_value_type(System_Type type) { return type != NULL && type->IsValueType; }
bool type_is_integer(System_Type type);

System_Type type_get_underlying_type(System_Type T);
System_Type type_get_verification_type(System_Type T);
System_Type type_get_intermediate_type(System_Type T);
bool type_is_array_element_compatible_with(System_Type T, System_Type U);
bool type_is_compatible_with(System_Type T, System_Type U);
bool type_is_verifier_assignable_to(System_Type Q, System_Type R);

bool isinstance(System_Object object, System_Type type);

