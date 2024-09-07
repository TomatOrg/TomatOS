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
 * Setup the per-cpu allocation
 * stack and enable running
 * on it when allocating
 */
err_t init_phys_per_cpu();

/**
 * Reclaim the bootloader memory since we are done
 */
void phys_reclaim_bootloader();

/**
 * Allocate physical memory, up to 128mb of contig memory
 */
void* phys_alloc(size_t size);

/**
 * Allocate physical memory before per-cpu data is initialized
 * this is needed to actually allocate the per-cpu data
 */
void* early_phys_alloc(size_t size);


/**
 * Free physical memory
 */
void phys_free(void* ptr);
