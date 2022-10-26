#pragma once

#include <util/except.h>
#include <util/defs.h>

#include "phys.h"

// size of a single phys
#define PAGE_SIZE                       (SIZE_4KB)

// The start of the higher half
#define HIGHER_HALF_START               (0xffff800000000000ull)

// contains direct mapping between virtual and physical addresses, we reserve
// a total of 512GB, this is enough for the newest intel cpus which have 39bits of
// physical memory
#define DIRECT_MAP_SIZE                 (SIZE_512GB)
#define DIRECT_MAP_START                (HIGHER_HALF_START)
#define DIRECT_MAP_END                  (HIGHER_HALF_START + DIRECT_MAP_SIZE)

// the buddy tree used for the physical allocator of the kernel
// This only needs ~64MB but we will give it twice as much just in case
#define BUDDY_TREE_SIZE                 (SIZE_128MB)
#define BUDDY_TREE_START                (DIRECT_MAP_END + SIZE_1GB)
#define BUDDY_TREE_END                  (BUDDY_TREE_START + BUDDY_TREE_SIZE)
STATIC_ASSERT(DIRECT_MAP_END < BUDDY_TREE_START);

// The stack pool area, have enough for 64k running threads....
#define STACK_POOL_SIZE                 ((SIZE_1MB * 3ull) * SIZE_64KB)
#define STACK_POOL_START                (DIRECT_MAP_END + SIZE_64GB)
#define STACK_POOL_END                  (STACK_POOL_START + STACK_POOL_SIZE)
STATIC_ASSERT(BUDDY_TREE_END < STACK_POOL_START);

// The virtual area used for the GC objects, total of 26 pools, each
// is 512GB, so total of 13TB of virtual memory
#define OBJECT_HEAP_START               (0xffff810000000000ull)
#define OBJECT_HEAP_END                 (0xffff810000000000ull + SIZE_1TB * 13)
STATIC_ASSERT(STACK_POOL_END < OBJECT_HEAP_START);

// This is the area the recursive paging exist on
#define RECURSIVE_PAGING_SIZE           (SIZE_512GB)
#define RECURSIVE_PAGING_START          (0xFFFFFF0000000000ull)
#define RECURSIVE_PAGING_END            (RECURSIVE_PAGING_START + RECURSIVE_PAGING_SIZE)
STATIC_ASSERT(OBJECT_HEAP_END < RECURSIVE_PAGING_START);

// The kernel heap area
#define KERNEL_HEAP_SIZE                (SIZE_4GB)
#define KERNEL_HEAP_START               (RECURSIVE_PAGING_END + SIZE_1GB)
#define KERNEL_HEAP_END                 (KERNEL_HEAP_START + KERNEL_HEAP_SIZE)
STATIC_ASSERT(RECURSIVE_PAGING_END < KERNEL_HEAP_START);

// This is where the kernel virtual address is
#define KERNEL_BASE                     (0xffffffff80000000ull)
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

#include "phys.h"
#include "malloc.h"
#include "vmm.h"
