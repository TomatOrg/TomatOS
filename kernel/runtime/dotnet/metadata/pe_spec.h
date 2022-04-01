#pragma once

#include "metadata_spec.h"

#include <util/defs.h>

#include <stdint.h>

typedef struct pe_file_header {
    uint16_t machine;
#define PE_FILE_HEADER_MACHINE 0x14c
    uint16_t number_of_sections;
    uint32_t time_date_stamp;
    uint32_t pointer_to_symbol_table;
    uint32_t number_of_symbols;
    uint16_t optional_header_size;
    uint16_t characteristics;
#define     IMAGE_FILE_RELOCS_STRIPPED 0x0001
#define     IMAGE_FILE_EXECUTABLE_IMAGE 0x0002
#define     IMAGE_FILE_32BIT_MACHINE 0x0100
#define     IMAGE_FILE_DLL 0x2000
} PACKED pe_file_header_t;
STATIC_ASSERT(sizeof(pe_file_header_t) == 20);

typedef struct pe_directory {
    uint32_t rva;
    uint32_t size;
} PACKED pe_directory_t;
STATIC_ASSERT(sizeof(pe_directory_t) == 8);

typedef struct pe_optional_header {
    // Standard fields
    uint16_t magic;
    uint8_t major_linker_version;
    uint8_t minor_linker_version;
    uint32_t code_size;
    uint32_t initialized_data_size;
    uint32_t uninitialized_data_size;
    uint32_t entry_point_rva;
    uint32_t base_of_code;
    uint32_t base_of_data;

    // Windows NT specific fields
    uint32_t image_base;
    uint32_t section_alignment;
    uint32_t file_alignment;
    uint16_t os_major;
    uint16_t os_minor;
    uint16_t user_major;
    uint16_t user_minor;
    uint16_t subsys_major;
    uint16_t subsys_minor;
    uint32_t _reserved;
    uint32_t image_size;
    uint32_t header_size;
    uint32_t file_checksum;
    uint16_t subsystem;
    uint16_t dll_flags;
    uint32_t stack_reserve_size;
    uint32_t stack_commit_size;
    uint32_t heap_reserve_size;
    uint32_t heap_commit_size;
    uint32_t loader_flags;
    uint32_t number_of_data_directories;

    // Data directories
    pe_directory_t export_table;
    pe_directory_t import_table;
    pe_directory_t resource_table;
    pe_directory_t exception_table;
    pe_directory_t certificate_table;
    pe_directory_t base_relocation_table;
    pe_directory_t debug;
    pe_directory_t copyright;
    pe_directory_t global_ptr;
    pe_directory_t tls_table;
    pe_directory_t load_config_table;
    pe_directory_t bound_import;
    pe_directory_t iat;
    pe_directory_t delay_import_descriptor;
    pe_directory_t cli_header;
    pe_directory_t _reserved2;
} PACKED pe_optional_header_t;
STATIC_ASSERT(sizeof(pe_optional_header_t) == 28 + 68 + 128);

typedef struct pe_section_header {
    char name[8];
    uint32_t virtual_size;
    uint32_t virtual_address;
    uint32_t size_of_raw_data;
    uint32_t pointer_to_raw_data;
    uint32_t pointer_to_relocations;
    uint32_t pointer_to_line_numbers;
    uint16_t number_of_relocations;
    uint16_t number_of_line_numbers;
    uint32_t characteristics;
#define     IMAGE_SCN_CNT_CODE 0x00000020
#define     IMAGE_SCN_CNT_INITIALIZED_DATA 0x00000040
#define     IMAGE_SCN_CNT_UNINITIALIZED_DATA 0x00000080
#define     IMAGE_SCN_MEM_EXECUTE 0x20000000
#define     IMAGE_SCN_MEM_READ 0x40000000
#define     IMAGE_SCN_MEM_WRITE 0x80000000
} PACKED pe_section_header_t;
STATIC_ASSERT(sizeof(pe_section_header_t) == 40);

typedef struct pe_cli_header {
    uint32_t cb;
    uint16_t major_runtime_version;
    uint16_t minor_runtime_version;
    pe_directory_t metadata;
    uint32_t flags;
#define COMIMAGE_FLAGS_ILONLY 0x00000001
#define COMIMAGE_FLAGS_32BITREQUIRED 0x00000002
#define COMIMAGE_FLAGS_STRONGNAMESIGNED 0x00000008
#define COMIMAGE_FLAGS_NATIVE_ENTRYPOINT 0x00000010
#define COMIMAGE_FLAGS_TRACKDEBUGDATA 0x00010000
    token_t entry_point_token;
    pe_directory_t resources;
    pe_directory_t strong_name_signature;
    pe_directory_t code_manager_table;
    pe_directory_t vtable_fixups;
    pe_directory_t export_address_table_jump;
    pe_directory_t managed_native_header;
} PACKED pe_cli_header_t;
STATIC_ASSERT(sizeof(pe_cli_header_t) == 72);
