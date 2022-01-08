#pragma once

#include <util/list.h>
#include <stddef.h>
#include <util/except.h>

/**
 * Init phys allocator
 */
err_t init_palloc();

/**
 * Reclaim bootloader memory
 */
err_t palloc_reclaim();

void* palloc(size_t size);

void pfree(void* base);
