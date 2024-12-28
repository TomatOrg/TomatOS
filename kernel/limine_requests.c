#include "limine_requests.h"

#include <stdbool.h>
#include <stddef.h>
#include <debug/log.h>
#include <lib/except.h>

__attribute__((section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);


__attribute__((used, section(".limine_requests_start_marker")))
static volatile LIMINE_REQUESTS_START_MARKER;

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

__attribute__((section(".limine_requests")))
volatile struct limine_module_request g_limine_module_request = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 1, .response = NULL
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
    .flags = 0
};

__attribute__((used, section(".limine_requests_end_marker")))
static volatile LIMINE_REQUESTS_END_MARKER;

void limine_check_revision() {

    // basic boot information
    if (g_limine_bootloader_info_request.response != NULL) {
        LOG_DEBUG("Bootloader: %s - %s",
            g_limine_bootloader_info_request.response->name,
            g_limine_bootloader_info_request.response->version);
    }

    if (LIMINE_LOADED_BASE_REV_VALID == true) {
        LOG_DEBUG("Bootloader has loaded us using base revision %d", LIMINE_LOADED_BASE_REVISION);
    }

    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        ASSERT(!"Limine base revision not supported");
    }

}
