#pragma once

#include <util/list.h>
#include <stddef.h>
#include <util/except.h>

/**
 * Init phys allocator
 */
err_t init_palloc();

void* palloc(size_t size);

void pfree(void* base);
