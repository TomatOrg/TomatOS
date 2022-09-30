#pragma once

#include <limine.h>

#include <stdint.h>
#include <stddef.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// All the requests the kernel does so other stuff can use it
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern volatile struct limine_bootloader_info_request g_limine_bootloader_info;
extern volatile struct limine_kernel_file_request g_limine_kernel_file;
extern volatile struct limine_module_request g_limine_module;
extern volatile struct limine_smp_request g_limine_smp;
extern volatile struct limine_memmap_request g_limine_memmap;
extern volatile struct limine_rsdp_request g_limine_rsdp;
extern volatile struct limine_kernel_address_request g_limine_kernel_address;
extern volatile struct limine_framebuffer_request g_limine_framebuffer;

extern struct limine_file g_default_font;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Normal kernel services
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Get the amount of cpus this machine has
 */
int get_cpu_count();
