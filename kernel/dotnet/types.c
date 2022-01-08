#include "types.h"

#include "assembly.h"

#include <util/string.h>
#include <stdalign.h>
#include <stddef.h>

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

    cleanup:
    return err;
}
