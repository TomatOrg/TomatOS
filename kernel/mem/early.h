#pragma once

#include <kernel.h>
#include <stddef.h>

/**
 * Early allocation function, this is used as long as the stivale2 page tables
 * are used in the kernel, it allocates physical memory directly from the memory
 * map, so it removes it from the memory map.
 */
uintptr_t early_alloc_page_phys();
