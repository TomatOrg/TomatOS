#pragma once

#include <util/except.h>
#include <util/defs.h>

#include "phys.h"

// size of a single phys
#define PAGE_SIZE                       (SIZE_4KB)

// The start of the higher half
#define HIGHER_HALF_START               (0xffff800000000000ull)

// contains direct mapping between virtual and physical addresses, we reserve
// a total of 64tb for the whole physical address space (which should be enough...)
#define DIRECT_MAP_SIZE                 (SIZE_16TB)
#define DIRECT_MAP_START                (HIGHER_HALF_START)
#define DIRECT_MAP_END                  (HIGHER_HALF_START + DIRECT_MAP_SIZE)

// the buddy tree used for the physical allocator of the kernel
#define BUDDY_TREE_SIZE                 (SIZE_256GB) /* Should be more than enough for at least 16TB of physical memory */
#define BUDDY_TREE_START                (DIRECT_MAP_END + SIZE_1TB)
#define BUDDY_TREE_END                  (BUDDY_TREE_START + BUDDY_TREE_SIZE)

// This is the area the recursive paging exist on
#define RECURSIVE_PAGING_SIZE           (SIZE_512GB)
#define RECURSIVE_PAGING_START          (0xFFFFFF0000000000ull)
#define RECURSIVE_PAGING_END            (RECURSIVE_PAGING_START + RECURSIVE_PAGING_SIZE)

// The stack pool area, have enough for 64k running threads....
#define STACK_POOL_SIZE                 ((SIZE_1MB * 3ull) * SIZE_64KB)
#define STACK_POOL_START                (RECURSIVE_PAGING_END + SIZE_1GB)
#define STACK_POOL_END                  (STACK_POOL_START + STACK_POOL_SIZE)

// The kernel heap area
#define KERNEL_HEAP_SIZE                (SIZE_4GB)
#define KERNEL_HEAP_START               (STACK_POOL_END + SIZE_1GB)
#define KERNEL_HEAP_END                 (KERNEL_HEAP_START + KERNEL_HEAP_SIZE)

// This is where the kernel virtual address is
#define KERNEL_BASE                     (0xffffffff80000000)

// verify we have no overlaps
STATIC_ASSERT(BUDDY_TREE_END < RECURSIVE_PAGING_START);
STATIC_ASSERT(KERNEL_HEAP_END < KERNEL_BASE);

#define PHYS_TO_DIRECT(x) \
    ({ \
        uintptr_t _x = (uintptr_t)(x); \
        ASSERT(_x <= DIRECT_MAP_SIZE); \
        (void*)(_x + DIRECT_MAP_START); \
    })

#define DIRECT_TO_PHYS(x) \
    ({ \
        uintptr_t _x = (uintptr_t)(x); \
        ASSERT(DIRECT_MAP_START <= _x && _x <= DIRECT_MAP_END); \
        _x - DIRECT_MAP_START; \
    })
