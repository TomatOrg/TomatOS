#pragma once

#include <stddef.h>

#include "memory.h"
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
 * Allocate physical memory, up to 128mb of contig memory
 */
void* phys_alloc(size_t size);

/**
 * Free physical memory
 */
void phys_free(void* ptr);

/**
 * Dump all the buddies, for debug
 */
void phys_dump_buddy();
