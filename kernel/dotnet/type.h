#pragma once

#include "dotnet.h"

#include <stdbool.h>
#include <sync/spinlock.h>

struct type {
    // The Assembly in which the type is declared. For generic types, gets the Assembly in which
    // the generic type is defined.
    assembly_t assembly;

    // The attributes associated with the Type.
    uint32_t attributes;

    // The type from which the current Type directly inherits.
    type_t base_type;

    // TODO: declaring method

    // The type that declares the current nested type or generic type parameter.
    type_t declaring_type;

    // TODO: doc this
    const char* name;
    const char* namespace;

    // Returns the Type of the object encompassed or referred to by the current array,
    // pointer or reference type.
    type_t element_type;

    // A value that indicates whether the type is an array.
    bool is_array;

    // A value indicating whether the Type is passed by reference.
    bool is_by_ref;

    // A value indicating whether the Type is a pointer.
    bool is_pointer;

    // A value indicating whether the Type is one of the primitive types.
    bool is_primitive;

    // A value indicating whether the Type is a value type.
    bool is_value_type;

    // All the methods of the current Type.
    method_info_t methods;
    int methods_count;

    // All the fields of the current Type
    field_info_t fields;
    int fields_count;

    // other info which is not canonical to the System.Type
    int managed_size;
    int managed_alignment;
    int stack_size;
    int stack_alignment;

    // initialization flags
    uint8_t inited_size : 1;

    spinlock_t pointer_type_lock;
    type_t pointer_type;

    spinlock_t array_type_lock;
    type_t array_type;

    spinlock_t by_ref_type_lock;
    type_t by_ref_type;
};

/**
 * Returns a Type object that represents an array of the current type.
 */
type_t make_array_type(type_t type);

/**
 * Returns a Type object that represents the current type when passed as a ref
 * parameter (ByRef parameter in Visual Basic).
 */
type_t make_by_ref_type(type_t type);

/**
 * Returns a Type object that represents a pointer to the current type.
 */
type_t make_pointer_type(type_t type);

/**
 * Checks if the type is assignable from the given type
 */
bool type_is_assignable_from(type_t to, type_t from);
