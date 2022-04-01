#include "pe.h"

#include <util/string.h>
#include <mem/mem.h>

#define READ32(addr) (*((uint32_t*)(addr)))

/**
 * Get the RVA data as a new heap allocated buffer
 */
void* pe_get_rva_data(pe_file_t* ctx, pe_directory_t directory) {
    for (int i = 0; i < ctx->section_header_count; i++) {
        pe_section_header_t* header = &ctx->section_headers[i];

        // make sure it is in the section
        if (directory.rva < header->virtual_address) continue;
        if (directory.rva + directory.size > header->virtual_address + header->virtual_size) continue;

        // it is, allocate the data for it
        void* ptr = malloc(directory.size);
        if (ptr == NULL) return NULL;

        // get the raw values
        size_t offset = (directory.rva - header->virtual_address);
        size_t raw_offset = header->pointer_to_raw_data + offset;
        size_t raw_size = header->size_of_raw_data - offset;

        // calc the amount to copy and copy it
        size_t size_to_copy = directory.size;
        if (size_to_copy > raw_size) {
            size_to_copy = raw_size;
        }
        memcpy(ptr, ctx->file + raw_offset, size_to_copy);

        // return the data itself, after allocation
        return ptr;
    }
    return NULL;
}

/**
 * Resolve the RVA as a pointer
 */
const void* pe_get_rva_ptr(pe_file_t* ctx, pe_directory_t* directory) {
    for (int i = 0; i < ctx->section_header_count; i++) {
        pe_section_header_t* header = &ctx->section_headers[i];

        if (header->virtual_address <= directory->rva && directory->rva < header->virtual_address + header->virtual_size) {
            size_t offset = (directory->rva - header->virtual_address);
            size_t raw_offset = header->pointer_to_raw_data + offset;
            size_t raw_size = header->size_of_raw_data - offset;

            // set the size we have left and return the pointer
            directory->size = raw_size;
            return ctx->file + raw_offset;
        }
    }
    return NULL;
}

/**
 * Does all of the PE parsing the get the sections and to get the cli header
 */
err_t pe_parse(pe_file_t* ctx) {
    err_t err = NO_ERROR;
    pe_cli_header_t* cli_header = NULL;

    // Get the lfanew and verify it
    CHECK(0x3c < ctx->file_size);
    uint32_t lfanew = READ32(ctx->file + 0x3c);
    uint32_t sections_offset = lfanew + 4 + sizeof(pe_file_header_t) + sizeof(pe_optional_header_t);
    CHECK(sections_offset < ctx->file_size);

    // check the signature
    CHECK(ctx->file[lfanew + 0] == 'P');
    CHECK(ctx->file[lfanew + 1] == 'E');
    CHECK(ctx->file[lfanew + 2] == '\0');
    CHECK(ctx->file[lfanew + 3] == '\0');

    // get the pe header and verify it
    pe_file_header_t* file_header = (pe_file_header_t*)&ctx->file[lfanew + 4];
    CHECK(file_header->machine == 0x14c);
    CHECK(file_header->optional_header_size == sizeof(pe_optional_header_t));
    CHECK(!(file_header->characteristics & IMAGE_FILE_RELOCS_STRIPPED));
    CHECK(file_header->characteristics & IMAGE_FILE_EXECUTABLE_IMAGE);

    // check the optional header, we ignore os, user and subsys versions for now
    pe_optional_header_t* optional_header = (pe_optional_header_t*)&ctx->file[lfanew + 4 + sizeof(pe_file_header_t)];
    CHECK(optional_header->magic == 0x10B);
    CHECK(optional_header->image_base % 0x10000 == 0);
    CHECK(optional_header->section_alignment > optional_header->file_alignment);
    CHECK(optional_header->file_alignment == 0x200);
    // TODO: heap size
    // TODO: stack size
    CHECK(optional_header->loader_flags == 0);
    CHECK(optional_header->number_of_data_directories == 0x10);

    // Verify the sections size and get the section headers
    CHECK(sections_offset + file_header->number_of_sections * sizeof(pe_section_header_t) < ctx->file_size);
    ctx->section_header_count = file_header->number_of_sections;
    ctx->section_headers = (pe_section_header_t*)&ctx->file[sections_offset];

    // verify section headers are all within the binary
    for (int i = 0; i < ctx->section_header_count; i++) {
        CHECK(ctx->section_headers[i].size_of_raw_data % optional_header->file_alignment == 0);
        CHECK(ctx->section_headers[i].pointer_to_raw_data % optional_header->file_alignment == 0);
        CHECK(ctx->section_headers[i].pointer_to_raw_data + ctx->section_headers[i].size_of_raw_data <= ctx->file_size);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Parse the CLI header
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // get and verify the cli header
    size_t cli_header_size = optional_header->cli_header.size;
    CHECK(cli_header_size >= sizeof(pe_cli_header_t));
    cli_header = pe_get_rva_data(ctx, optional_header->cli_header);
    CHECK(cli_header != NULL);
    CHECK(cli_header->cb == sizeof(pe_cli_header_t));
    CHECK(cli_header->major_runtime_version == 2);
    CHECK(cli_header->minor_runtime_version == 5); // standard says 0, this is actually 5 :shrug:
    CHECK(cli_header->flags & COMIMAGE_FLAGS_ILONLY);
    CHECK(!(cli_header->flags & COMIMAGE_FLAGS_32BITREQUIRED));
    CHECK(!(cli_header->flags & COMIMAGE_FLAGS_NATIVE_ENTRYPOINT));
    CHECK(!(cli_header->flags & COMIMAGE_FLAGS_TRACKDEBUGDATA));

    // set it
    ctx->cli_header = cli_header;

cleanup:
    if (IS_ERROR(err)) {
        free_pe_file(ctx);
    }

    return err;
}

void free_pe_file(pe_file_t* ctx) {
    free(ctx->cli_header);
    memset(ctx, 0, sizeof(*ctx));
}
