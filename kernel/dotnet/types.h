#pragma once

#include <stddef.h>
#include <stdint.h>
#include "util/list.h"

typedef struct object object_t;
typedef struct type type_t;

/**
 * Represents a dotnet object
 */
struct object {
    // the type of the object, must be first
    type_t* type;

    // TODO: other stuff that we want first for speed reasons
    //       for example vtable, scanning functions, or whatever else

    //
    // GC related stuff
    //
    object_t** log_pointer;

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
            object_t* next;

            // next chunk
            object_t* chunk_next;
        };

        // next allocated object
        // TODO: rcu? is this even a valid case for rcu?
        list_entry_t entry;
    };
};

typedef struct member_info {
    object_t object;

    type_t* declaring_type;
    wchar_t* name;
    int32_t attributes;
    // TODO: custom attributes
} member_info_t;

typedef struct abstract_type {
    member_info_t member_info;
} abstract_type_t;

typedef struct field_info {
    member_info_t member_info;
} field_info_t;

typedef struct runtime_field_info {
    field_info_t field_info;
    type_t* field_type;
    size_t offset;
} runtime_field_info_t;

typedef struct runtime_parameter_info {
    // TODO: object? what is this?
    int32_t attributes;
    abstract_type_t* parameter_type;
    void* default_value;
} runtime_parameter_info_t;

typedef struct method_base {
    member_info_t member_info;
    runtime_parameter_info_t* const* parameters;
    // TODO: invoke
} method_base_t;

typedef struct constructor_info {
    method_base_t method_base;
} constructor_info_t;

typedef struct runtime_constructor_info {
    constructor_info_t constructor_info;
    // TODO: create
} runtime_constructor_info_t;

typedef struct method_info {
    method_base_t method_base;
} method_info_t;

typedef struct runtime_method_info {
    void* function;
    method_info_t method_info;
    struct runtime_method_info* generic_definition;
    abstract_type_t* const* generic_arguments;
    struct runtime_method_info* const* generic_methods;
} runtime_method_info_t;

typedef struct property_info {
    member_info_t member_info;
} property_info_t;

typedef struct runtime_property_info {
    property_info_t property_info;
    runtime_parameter_info_t* const* parameters;
    runtime_method_info_t* get;
    runtime_method_info_t* set;
} runtime_property_info_t;

typedef struct assembly {
    object_t object;
} assembly_t;

typedef struct runtime_assembly {
    assembly_t assembly;

    wchar_t* full_name;
    wchar_t* name;
    runtime_method_info_t* entry_point;
    type_t* const* exported_types;
} runtime_assembly_t;

/**
 * Represents a dotnet type
 */
struct type {
    abstract_type_t abstract_type;

    type_t* base;
    type_t* const* interfaces;
    // TODO: interface_to_methods
    assembly_t* assembly;
    wchar_t* namespace;
    wchar_t* full_name;
    wchar_t* display_name;
    uint8_t is_managed : 1;
    uint8_t is_value_type : 1;
    uint8_t is_array : 1;
    uint8_t is_enum : 1;
    uint8_t is_by_ref : 1;
    uint8_t is_pointer : 1;
    uint8_t is_by_ref_like : 1;
    uint8_t _reserved : 1;
    uint8_t cor_element_type;
    uint8_t type_code;
    size_t size;
    size_t managed_size;
    size_t unmanaged_size;
    size_t slots;
    type_t* szarray;
    union {
        struct {
            type_t* element;
            size_t rank;
        };
        struct {
            void* multicast_invoke;
            void* invoke_static;
            void* invoke_unmanaged;
        };
        type_t* underlying;
    };
    type_t* generic_definition;
    abstract_type_t* const* generic_arguments;
    type_t* const* generic_types;
    runtime_field_info_t* const* fields;
    runtime_constructor_info_t* const* constructors;
    runtime_method_info_t* const* methods;
    runtime_property_info_t* const* properties;

    //
    // GC information
    //

    // array of offsets into the structures that contains
    // managed pointers that the gc needs to worry about
    size_t* managed_pointer_offsets;
};

typedef struct generic_parameter {
    abstract_type_t abstract_type;
    int32_t parameter_attributes;
    int32_t position;
} generic_parameter_t;

typedef struct generic_type_parameter {
    generic_parameter_t generic_parameter;
} generic_type_parameter_t;

typedef struct generic_method_parameter {
    generic_parameter_t generic_parameter;
} generic_method_parameter_t;

