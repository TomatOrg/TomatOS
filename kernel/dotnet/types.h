#pragma once

#include <stddef.h>
#include <stdint.h>
#include "util/list.h"
#include "util/except.h"
#include "sync/spinlock.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This file defines all the types that the kernel relies upon for proper operation of the dotnet runtime, internally
// we use the reflection structure for everything we need, and they have the exact same layout as they have in the
// C# code, that allows us to properly use the garbage collector without needing to do anything ourselves since all
// the offsets will be set nicely for us when parsing the initial types
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct System_Object System_Object;
typedef struct System_Type System_Type;

/**
 * Represents a dotnet object
 */
struct System_Object {
    // the type of the object, must be first
    System_Type* Type;

    // TODO: other stuff that we want first for speed reasons
    //       for example vtable, scanning functions, or whatever else

    //
    // GC related stuff
    //
    System_Object** log_pointer;

    // the color of the object, black and white switch during collection
    // and blue means unallocated
    uint8_t color;

    // the rank of the object from the allocator
    uint8_t rank;

    uint8_t _reserved0;
    uint8_t _reserved1;

    union {
        struct {
            // next free object in the chunk
            System_Object* next;

            // next chunk
            System_Object* chunk_next;
        };

        // next allocated object
        // TODO: rcu? is this even a valid case for rcu?
        list_entry_t entry;
    };
};

typedef struct System_Array {
    System_Object Object;
    int Length;
    char data[];
} System_Array;

#define SYSTEM_ARRAY(type, array) ((type*)(array)->data)

#define SYSTEM_ARRAY_SIZE(typ, count) (sizeof(System_Array) + sizeof(typ) * (count))

typedef struct System_String {
    System_Object Object;
    int Length;
    wchar_t Chars[];
} System_String;

typedef struct System_Reflection_Assembly {
    System_Object Object;
    System_Array* DefinedTypes;
} System_Reflection_Assembly;

typedef struct System_Reflection_Module {
    System_Object Object;
    System_Reflection_Assembly* Assembly;
    System_String* Name;
} System_Reflection_Module;

typedef struct System_Reflection_MemberInfo {
    System_Object Object;
    System_Type* DeclaringType;
    System_String* Name;
    System_Reflection_Module* Module;
} System_Reflection_MemberInfo;

typedef struct System_Reflection_FieldInfo {
    System_Reflection_MemberInfo MemberInfo;
    uint16_t Attributes;
    System_Type* FieldType;
} System_Reflection_FieldInfo;

struct System_Type {
    System_Object Object;
    System_Reflection_Assembly* Assembly;
    System_String* Name;
    System_String* Namespace;

    // Fields and methods
    System_Array* Fields;
    System_Array* Methods;

    // The element type, relevant for anything 
    System_Type* ElementType;

    // the array type for this type, can be null
    System_Type* ArrayType;
    spinlock_t TypeLock;
};

/**
 * Create a System.String object from a null terminated c string
 *
 * @param string    [OUT]   The new string object
 * @param str       [IN]    The c string
 */
System_String* new_string_from_cstr(const char* str);

/**
 * Get an array type for the given base type
 *
 * @param baseType      [IN]    The base type
 */
System_Type* get_array_type(System_Type* baseType);
