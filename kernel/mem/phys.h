#pragma once

#include <stddef.h>
#include "lib/except.h"

/**
 * Initialize the physical memory allocator
 */
err_t init_phys();

/**
 * Map the physical mappings to the direct map
 */
err_t init_phys_mappings();

/**
 * Reclaim the bootloader memory since we are done
 */
void phys_reclaim_bootloader();

/**
 * Allocate a physical page
 */
void* phys_alloc_page();

/**
 * Free a physical pointer
 */
void phys_free_page(void* ptr);
