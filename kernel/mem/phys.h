#pragma once

#include <util/list.h>
#include <util/except.h>

#include <stddef.h>

/**
 * Init phys allocator
 */
err_t init_palloc();

/**
 * Reclaim bootloader memory
 */
err_t palloc_reclaim();

void* palloc(size_t size) __attribute__((alloc_size(1)));

/**
 * Allocate memory, this should only be called before per-cpu
 * memory is setup properly.
 */
void* early_palloc(size_t size) __attribute__((alloc_size(1)));

void pfree(void* base);
