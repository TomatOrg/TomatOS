#pragma once

#include <util/defs.h>

#include <stdint.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Basic types
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct guid {
    uint64_t low;
    uint64_t high;
} PACKED guid_t;
STATIC_ASSERT(sizeof(guid_t) == 16);

typedef union token {
    struct {
        uint32_t index : 24;
        uint32_t table : 8;
    };
    int token;
} PACKED token_t;
STATIC_ASSERT(sizeof(token_t) == sizeof(int));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Metadata entries
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct blob_entry {
    const uint8_t* data;
    size_t size;
} blob_entry_t;

#define METADATA_MODULE 0x00
typedef struct metadata_module {
    uint16_t generation;
    const char* name;
    guid_t* mvid;
    guid_t* enc_id;
    guid_t* enc_base_id;
} PACKED metadata_module_t;

#define METADATA_TYPE_REF 0x01
typedef struct metadata_type_ref {
    token_t resolution_scope;
    const char* type_name;
    const char* type_namespace;
} PACKED metadata_type_ref_t;

#define METADATA_TYPE_DEF 0x02
typedef struct metadata_type_def {
    uint32_t flags;
    const char* type_name;
    const char* type_namespace;
    token_t extends;
    token_t field_list;
    token_t method_list;
} PACKED metadata_type_def_t;

#define METADATA_FIELD 0x04
typedef struct metadata_field {
    uint16_t flags;
    const char* name;
    blob_entry_t signature;
} PACKED metadata_field_t;

#define METADATA_METHOD_DEF 0x06
typedef struct metadata_method_def {
    uint32_t rva;
    uint16_t impl_flags;
    uint16_t flags;
    const char* name;
    blob_entry_t signature;
    token_t param_list;
} PACKED metadata_method_def_t;

#define METADATA_PARAM 0x08
typedef struct metadata_param {
    uint16_t flags;
    uint16_t sequence;
    const char* name;
} PACKED metadata_param_t;

#define METADATA_INTERFACE_IMPL 0x09
typedef struct metadata_interface_impl {
    token_t class;
    token_t interface;
} PACKED metadata_interface_impl_t;

#define METADATA_MEMBER_REF 0x0a
typedef struct metadata_member_ref {
    token_t class;
    const char* name;
    blob_entry_t signature;
} PACKED metadata_member_ref_t;

#define METADATA_CONSTANT 0x0b
typedef struct metadata_constant {
    uint16_t type;
    token_t parent;
    blob_entry_t value;
} PACKED metadata_constant_t;

#define METADATA_CUSTOM_ATTRIBUTE 0x0c
typedef struct metadata_custom_attribute {
    token_t parent;
    token_t type;
    blob_entry_t value;
} PACKED metadata_custom_attribute_t;

#define METADATA_DECL_SECURITY 0x0e
typedef struct metadata_decl_security {
    uint16_t action;
    token_t parent;
    blob_entry_t permission_set;
} PACKED metadata_decl_security_t;

#define METADATA_CLASS_LAYOUT 0x0f
typedef struct metadata_class_layout {
    int16_t packing_size;
    int32_t class_size;
    token_t parent;
} PACKED metadata_class_layout_t;

#define METADATA_FIELD_LAYOUT 0x10
typedef struct metadata_Field_layout {
    uint32_t offset;
    token_t field;
} PACKED metadata_Field_layout_t;

#define METADATA_STAND_ALONE_SIG 0x11
typedef struct metadata_stand_alone_sig {
    blob_entry_t signature;
} PACKED metadata_stand_alone_sig_t;

#define METADATA_EVENT_MAP 0x12
typedef struct metadata_event_map {
    token_t parent;
    token_t event_list;
} PACKED metadata_event_map_t;

#define METADATA_EVENT 0x14
typedef struct metadata_event {
    uint16_t event_flags;
    const char* name;
    token_t event_type;
} PACKED metadata_event_t;

#define METADATA_PROPERTY_MAP 0x15
typedef struct metadata_property_map {
    token_t parent;
    token_t property_list;
} PACKED metadata_property_map_t;

#define METADATA_PROPERTY 0x17
typedef struct metadata_property {
    uint16_t flags;
    const char* name;
    blob_entry_t type;
} PACKED metadata_property_t;

#define METADATA_METHOD_SEMANTICS 0x18
typedef struct metadata_method_semantics {
    uint16_t semantics;
    token_t method;
    token_t association;
} PACKED metadata_method_semantics_t;

#define METADATA_METHOD_IMPL 0x19
typedef struct metadata_method_impl {
    token_t class;
    token_t method_body;
    token_t method_declaration;
} PACKED metadata_method_impl_t;

#define METADATA_TYPE_SPEC 0x1b
typedef struct metadata_type_spec {
    blob_entry_t signature;
} PACKED metadata_type_spec_t;

#define METADATA_ASSEMBLY 0x20
typedef struct metadata_assembly {
    uint32_t hash_alg_id;
    uint16_t major_version;
    uint16_t minor_version;
    uint16_t build_number;
    uint16_t revision_number;
    uint32_t flags;
    blob_entry_t public_key;
    const char* name;
    const char* culture;
} PACKED metadata_assembly_t;

#define METADATA_ASSEMBLY_REF 0x23
typedef struct metadata_assembly_ref {
    uint16_t major_version;
    uint16_t minor_version;
    uint16_t build_number;
    uint16_t revision_number;
    uint32_t flags;
    blob_entry_t public_key_or_token;
    const char* name;
    const char* culture;
    blob_entry_t hash_value;
} PACKED metadata_assembly_ref_t;

#define METADATA_ASSEMBLY_REF_OS 0x25
typedef struct metadata_assembly_ref_os {
    uint32_t os_platform_id;
    uint32_t os_major_version;
    uint32_t os_minor_version;
    token_t assembly_ref;
} PACKED metadata_assembly_ref_os_t;

#define METADATA_EXPORTED_TYPE 0x27
typedef struct metadata_exported_type {
    uint32_t flags;
    uint32_t type_def_id;
    const char* type_name;
    const char* type_namespace;
    token_t implementation;
} PACKED metadata_exported_type_t;

#define METADATA_NESTED_CLASS 0x29
typedef struct metadata_nested_class {
    token_t nested_class;
    token_t enclosing_class;
} PACKED metadata_nested_class_t;

#define METADATA_GENERIC_PARAM 0x2a
typedef struct metadata_generic_param {
    uint16_t number;
    uint16_t flags;
    token_t owner;
    const char* name;
} PACKED metadata_generic_param_t;

#define METADATA_GENERIC_PARAM_CONSTRAINT 0x2c
typedef struct metadata_generic_param_constraint {
    token_t owner;
    token_t constraint;
} PACKED metadata_generic_param_constraint_t;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Method header
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define CorILMethod_TinyFormat      0x2
#define CorILMethod_FatFormat       0x3
#define CorILMethod_MoreSects       0x8
#define CorILMethod_InitLocals      0x10

#define CorILMethod_Sect_EHTable    0x1
#define CorILMethod_Sect_OptILTable 0x2
#define CorILMethod_Sect_KindMask   0x3F
#define CorILMethod_Sect_FatFormat  0x40
#define CorILMethod_Sect_MoreSects  0x80

#define COR_ILEXCEPTION_CLAUSE_EXCEPTION    0x0000
#define COR_ILEXCEPTION_CLAUSE_FILTER       0x0001
#define COR_ILEXCEPTION_CLAUSE_FINALLY      0x0002
#define COR_ILEXCEPTION_CLAUSE_FAULT        0x0004

typedef struct method_tiny_format {
    uint8_t flags : 2;
    uint8_t size : 6;
} PACKED method_tiny_format_t;

typedef struct method_fat_format {
    uint16_t flags : 12;
    uint16_t size : 4;
    uint16_t max_stack;
    uint32_t code_size;
    token_t local_var_sig_tok;
} PACKED method_fat_format_t;

typedef struct method_section_tiny {
    uint8_t flags;
    uint8_t size;
} PACKED method_section_tiny_t;

typedef struct method_section_fat {
    uint32_t flags : 8;
    uint32_t size : 24;
} PACKED method_section_fat_t;

typedef struct method_exception_clause {
    uint16_t flags;
    uint16_t try_offset;
    uint8_t try_length;
    uint16_t handler_offset;
    uint8_t handler_length;
    union {
        token_t class_token;
        int filter_offset;
    };
} PACKED method_exception_clause_t;
STATIC_ASSERT(sizeof(method_exception_clause_t) == 12);

typedef struct method_fat_exception_clause {
    int flags;
    int try_offset;
    int try_length;
    int handler_offset;
    int handler_length;
    union {
        token_t class_token;
        int filter_offset;
    };
} PACKED method_fat_exception_clause_t;
STATIC_ASSERT(sizeof(method_fat_exception_clause_t) == 24);
