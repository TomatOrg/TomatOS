#include "limine_requests.h"

#include <stdbool.h>
#include <stddef.h>
#include <debug/log.h>
#include <lib/except.h>

//
// Metadata for limine to find our requests
//

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

//
// The actual requests
//

__attribute__((section(".limine_requests")))
volatile struct limine_framebuffer_request g_limine_framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0, .response = NULL
};

__attribute__((section(".limine_requests")))
volatile struct limine_bootloader_info_request g_limine_bootloader_info_request = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST,
    .revision = 0, .response = NULL
};

__attribute__((section(".limine_requests")))
volatile struct limine_hhdm_request g_limine_hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0, .response = NULL
};


__attribute__((section(".limine_requests")))
volatile struct limine_memmap_request g_limine_memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0, .response = NULL
};

__attribute__((section(".limine_requests")))
volatile struct limine_executable_file_request g_limine_executable_file_request = {
    .id = LIMINE_EXECUTABLE_FILE_REQUEST,
    .revision = 0, .response = NULL
};

static struct limine_internal_module* m_internal_modules[] = {
    // The corelib, must be present
    &(struct limine_internal_module){
        .path = "/System.Private.CoreLib.dll",
        .flags = LIMINE_INTERNAL_MODULE_REQUIRED
    },

    // The kernel itself, must be present
    &(struct limine_internal_module){
        .path = "/Tomato.Kernel.dll",
        .flags = LIMINE_INTERNAL_MODULE_REQUIRED
    },
};

__attribute__((section(".limine_requests")))
volatile struct limine_module_request g_limine_module_request = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 1, .response = NULL,
    .internal_modules = m_internal_modules,
    .internal_module_count = ARRAY_LENGTH(m_internal_modules)
};

__attribute__((section(".limine_requests")))
volatile struct limine_rsdp_request g_limine_rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
    .revision = 0, .response = NULL
};

__attribute__((section(".limine_requests")))
volatile struct limine_executable_address_request g_limine_executable_address_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST,
    .revision = 0, .response = NULL
};

__attribute__((section(".limine_requests")))
volatile struct limine_mp_request g_limine_mp_request = {
    .id = LIMINE_MP_REQUEST,
    .revision = 0, .response = NULL,
    .flags = LIMINE_MP_X2APIC
};

void limine_check_revision() {

    // basic boot information
    if (g_limine_bootloader_info_request.response != NULL) {
        TRACE("Bootloader: %s - %s",
            g_limine_bootloader_info_request.response->name,
            g_limine_bootloader_info_request.response->version);
    }

    if (LIMINE_LOADED_BASE_REV_VALID == true) {
        TRACE("Bootloader has loaded us using base revision %lu", LIMINE_LOADED_BASE_REVISION);
    }

    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        ASSERT(!"Limine base revision not supported");
    }

}
