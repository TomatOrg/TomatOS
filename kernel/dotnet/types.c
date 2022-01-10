#include "types.h"

#include "assembly.h"

#include <util/string.h>
#include <stdalign.h>
#include <stddef.h>
#include <dotnet/builtin/string.h>

type_t g_void = NULL;

type_t g_sbyte = NULL;
type_t g_byte = NULL;
type_t g_short = NULL;
type_t g_ushort = NULL;
type_t g_int = NULL;
type_t g_uint = NULL;
type_t g_long = NULL;
type_t g_ulong = NULL;
type_t g_nint = NULL;
type_t g_nuint = NULL;

type_t g_float = NULL;
type_t g_double = NULL;
type_t g_decimal = NULL;

type_t g_bool = NULL;
type_t g_char = NULL;

type_t g_string = NULL;
type_t g_object = NULL;
type_t g_value_type = NULL;
type_t g_array = NULL;

type_t g_arithmetic_exception = NULL;
type_t g_overflow_exception = NULL;
type_t g_null_reference_exception = NULL;
type_t g_divide_by_zero_exception = NULL;

typedef struct base_type {
    type_t* pointer;
    const char* namespace;
    const char* name;
    size_t size;
    size_t alignment;
} base_type_t;

#define PRIMITIVE_TYPE(ptr, cstype, ctype) \
    { .pointer = &ptr, .namespace = "System", .name = cstype, .size = sizeof(ctype), .alignment = alignof(ctype) }

#define TYPE(ptr, cstype) \
    { .pointer = &ptr, .namespace = "System", .name = cstype, .size = -1, .alignment = -1 }

static base_type_t m_base_types[] = {
        TYPE(g_void, "Void"),

        PRIMITIVE_TYPE(g_sbyte, "SByte", int8_t),
        PRIMITIVE_TYPE(g_byte, "Byte", uint8_t),
        PRIMITIVE_TYPE(g_short, "Int16", int16_t),
        PRIMITIVE_TYPE(g_ushort, "UInt16", uint16_t),
        PRIMITIVE_TYPE(g_int, "Int32", int32_t),
        PRIMITIVE_TYPE(g_uint, "UInt32", uint32_t),
        PRIMITIVE_TYPE(g_long, "Int64", int64_t),
        PRIMITIVE_TYPE(g_ulong, "UInt64", uint64_t),
        PRIMITIVE_TYPE(g_nint, "IntPtr", intptr_t),
        PRIMITIVE_TYPE(g_nuint, "UIntPtr", uintptr_t),

        PRIMITIVE_TYPE(g_float, "Single", float),
        PRIMITIVE_TYPE(g_double, "Double", double),

        PRIMITIVE_TYPE(g_bool, "Boolean", bool),
        PRIMITIVE_TYPE(g_char, "Char", wchar_t),

        TYPE(g_string, "String"),
        TYPE(g_object, "Object"),
        TYPE(g_value_type, "ValueType"),
        TYPE(g_array, "Array"),

        TYPE(g_arithmetic_exception, "ArithmeticException"),
        TYPE(g_overflow_exception, "OverflowException"),
        TYPE(g_null_reference_exception, "NullReferenceException"),
        TYPE(g_divide_by_zero_exception, "DivideByZeroException"),
};

err_t initialize_base_types(metadata_type_def_t* base_types, int base_types_count) {
    err_t err = NO_ERROR;

    int wanted_to_find = ARRAY_LEN(m_base_types);
    for (int i = 0; i < base_types_count; i++) {
        for (int j = 0; j < ARRAY_LEN(m_base_types); j++) {
            base_type_t* base = &m_base_types[j];

            // skip if already initialized
            if (*base->pointer != NULL) continue;

            // check if this is what we want
            if (strcmp(base->namespace, base_types[i].type_namespace) != 0) continue;
            if (strcmp(base->name, base_types[i].type_name) != 0) continue;

            // it is!
            *base->pointer = &g_corlib->types[i];
            wanted_to_find--;

            // set the size and alignment, if any, otherwise
            // this would have to be specified manually
            if (base->size != -1) {
                type_t type = *base->pointer;
                type->stack_size = base->size;
                type->stack_alignment = base->alignment;
                type->managed_size = base->size;
                type->managed_alignment = base->alignment;
                type->inited_size = true;

                // if we have a size in here this is a primitive type
                type->is_primitive = true;
                type->is_value_type = true;
            }
        }
    }

    // make sure we found all the types we wanted
    if (wanted_to_find != 0) {
        ERROR("Missing base types (%d):", wanted_to_find);
        for (int j = 0; j < ARRAY_LEN(m_base_types); j++) {
            base_type_t* base = &m_base_types[j];
            if (*base->pointer != NULL) continue;
            ERROR("\t%s.%s", base->namespace, base->name);
        }
        CHECK_FAIL("Could not find all base types!");
    }

    // All value types inherit from this type (ignoring primitive
    // types)
    g_value_type->is_value_type = true;

    // string is a built-in type
    g_string->managed_size = sizeof(system_string_t);
    g_string->managed_alignment = alignof(system_string_t);

cleanup:
    return err;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Type handling
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

type_t get_underlying_type(type_t T) {
    // TODO: T is enum
    return T;
}

type_t get_reduced_type(type_t T) {
    T = get_underlying_type(T);

    if (T->is_primitive) {
        if (T == g_byte) {
            return g_sbyte;
        } else if (T == g_ushort) {
            return g_short;
        } else if (T == g_uint) {
            return g_int;
        } else if (T == g_ulong) {
            return g_long;
        } else if (T == g_nuint) {
            return g_nint;
        }
    }

    return T;
}

type_t get_verification_type(type_t T) {
    if (T->is_by_ref) {
        // This is a managed reference, get the reduced type of the element
        type_t S = get_reduced_type(T->element_type);

        // convert bool/char to int8/int16 respectively
        if (S == g_bool) {
            return make_by_ref_type(g_sbyte);
        } else if (S == g_char) {
            return make_by_ref_type(g_short);
        }

        // make sure to use the reduced type for the new reference
        return make_by_ref_type(S);
    } else {
        // Use the reduced type
        T = get_reduced_type(T);

        // convert bool/char to int8/int16 respectively
        if (T == g_bool) {
            return g_sbyte;
        } else if (g_char) {
            return g_short;
        }
    }

    return T;
}

type_t get_intermediate_type(type_t T) {
    T = get_verification_type(T);
    if (T == g_sbyte || T == g_short || T == g_int) {
        return g_int;
    } else if (T == g_float || T == g_double) {
        // TODO: should be of type F, we are going to
        //       treat this as double
        return g_double;
    }
    return T;
}

type_t get_direct_base_class(type_t T) {
    if (T->is_array) {
        return g_array;
    }
    return NULL;
}

static bool is_signature_type_compatible_with(type_t T, type_t U);

static bool is_array_element_compatible_with(type_t T, type_t U) {
    type_t V = get_underlying_type(T);
    type_t W = get_underlying_type(U);

    if (is_signature_type_compatible_with(V, W)) {
        return true;
    } else if (get_reduced_type(V) == get_reduced_type(W)) {
        return true;
    }

    return false;
}

static bool is_signature_type_compatible_with(type_t T, type_t U) {
    if (T == U) {
        return true;
    }
        // TODO: 2
    else if (T->by_ref_type && get_direct_base_class(T) == U) {
        return true;
    }
    // TODO: 4
    else if (T->is_array && U->is_array && is_array_element_compatible_with(T->element_type, U->element_type)) {
        return true;
    }
    // TODO: 6
    // TODO: 7
    // TODO: 8
    // TODO: 9
    return false;
}

static bool is_pointer_elemenet_compatible_with(type_t T, type_t U) {
    type_t V = get_verification_type(T);
    type_t W = get_verification_type(U);
    return V == W;
}

/**
 * Also called location type in the spec
 */
static bool is_type_compatible_with(type_t T, type_t U) {
    if (!T->is_by_ref && !U->is_by_ref && is_signature_type_compatible_with(T, U)) {
        return true;
    } else if (T->is_by_ref && U->is_by_ref && is_pointer_elemenet_compatible_with(T, U)) {
        return true;
    }
    return false;
}

bool is_type_assignable_to(type_t T, type_t U) {
    if (T == U) {
        return true;
    }
    // TODO: 2
    else if (get_intermediate_type(T) == get_intermediate_type(U)) {
        return true;
    }
    else if (
        (get_intermediate_type(T) == g_nint && get_intermediate_type(U) == g_int) ||
        (get_intermediate_type(T) == g_int && get_intermediate_type(U) == g_nint)
    ) {
        return true;
    } else if (is_type_compatible_with(T, U)) {
        return true;
    }
}
