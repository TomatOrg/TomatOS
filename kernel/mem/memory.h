#pragma once

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The kernel memory map
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// 0x00000000_00000000 - 0x00000000_7FFFFFFF: Unmapped - any fault will cause a null-ref-exception
// 0x00000000_80000000 - 0x00000000_FFFFFFFF: 32bit allocator, used for anything that has pointers
//                                            that should be of 32bit size
//
// The rest of the lower half are reserved for WASM memory ranges
//
// 0xFFFF8000_00000000 - 0xFFFF807F_FFFFFFFF: Direct memory map
// 0xFFFF8080_00000000 - 0xFFFF80FF_FFFFFFFF: --- reserved ---
//
// 0xFFFF8100_00000000 - 0xFFFF817F_FFFFFFFF: GC Heap order 0  - 32
// 0xFFFF8180_00000000 - 0xFFFF81FF_FFFFFFFF: GC Heap order 1  - 64
// 0xFFFF8200_00000000 - 0xFFFF827F_FFFFFFFF: GC Heap order 2  - 128
// 0xFFFF8280_00000000 - 0xFFFF82FF_FFFFFFFF: GC Heap order 3  - 256
// 0xFFFF8300_00000000 - 0xFFFF837F_FFFFFFFF: GC Heap order 4  - 512
// 0xFFFF8380_00000000 - 0xFFFF83FF_FFFFFFFF: GC Heap order 5  - 1k
// 0xFFFF8400_00000000 - 0xFFFF847F_FFFFFFFF: GC Heap order 6  - 2k
// 0xFFFF8480_00000000 - 0xFFFF84FF_FFFFFFFF: GC Heap order 7  - 4k
// 0xFFFF8500_00000000 - 0xFFFF857F_FFFFFFFF: GC Heap order 8  - 8k
// 0xFFFF8580_00000000 - 0xFFFF85FF_FFFFFFFF: GC Heap order 9  - 16k
// 0xFFFF8600_00000000 - 0xFFFF867F_FFFFFFFF: GC Heap order 10 - 32k
// 0xFFFF8680_00000000 - 0xFFFF86FF_FFFFFFFF: GC Heap order 11 - 64k
// 0xFFFF8700_00000000 - 0xFFFF877F_FFFFFFFF: GC Heap order 12 - 128k
// 0xFFFF8780_00000000 - 0xFFFF87FF_FFFFFFFF: GC Heap order 13 - 256k
// 0xFFFF8800_00000000 - 0xFFFF887F_FFFFFFFF: GC Heap order 14 - 512k
// 0xFFFF8880_00000000 - 0xFFFF88FF_FFFFFFFF: GC Heap order 15 - 1m
// 0xFFFF8900_00000000 - 0xFFFF897F_FFFFFFFF: GC Heap order 16 - 2m
// 0xFFFF8980_00000000 - 0xFFFF89FF_FFFFFFFF: GC Heap order 17 - 4m
// 0xFFFF8A00_00000000 - 0xFFFF8A7F_FFFFFFFF: GC Heap order 18 - 8m
// 0xFFFF8A80_00000000 - 0xFFFF8AFF_FFFFFFFF: GC Heap order 19 - 16m
// 0xFFFF8B00_00000000 - 0xFFFF8B7F_FFFFFFFF: GC Heap order 20 - 32m
// 0xFFFF8B80_00000000 - 0xFFFF8BFF_FFFFFFFF: GC Heap order 21 - 64m
// 0xFFFF8C00_00000000 - 0xFFFF8C7F_FFFFFFFF: GC Heap order 22 - 128m
// 0xFFFF8C80_00000000 - 0xFFFF8CFF_FFFFFFFF: GC Heap order 23 - 256m
// 0xFFFF8D00_00000000 - 0xFFFF8D7F_FFFFFFFF: GC Heap order 24 - 512m
// 0xFFFF8D80_00000000 - 0xFFFF8DFF_FFFFFFFF: GC Heap order 25 - 1gb
// 0xFFFF8E00_00000000 - 0xFFFF8E7F_FFFFFFFF: GC Heap order 26 - 2gb
// 0xFFFF8E80_00000000 - 0xFFFF8EFF_FFFFFFFF: --- barrier ---
// 0xFFFF8F00_00000000 - 0xFFFF8F7F_FFFFFFFF: Stacks
// 0xFFFF8F80_00000000 - 0xFFFF8FFF_FFFFFFFF: Managed Mappings
//
// 0xFFFF9000_00000000 - 0xFFFF9FFF_FFFFFFFF: RO copy of the entire heap + RW copy of the stacks and managed mappings
//
// 0xFFFFA000_00000000 - 0xFFFFA07F_FFFFFFFF: Thread structs
//
// 0xFFFFFF00_00000000 - 0xFFFFFF7F_FFFFFFFF: Page Mapping Level 1 (Page Tables)
// 0xFFFFFF7F_80000000 - 0xFFFFFF7F_BFFFFFFF: Page Mapping Level 2 (Page Directories)
// 0xFFFFFF7F_BFC00000 - 0xFFFFFF7F_BFDFFFFF: Page Mapping Level 3 (PDPTs / Page-Directory-Pointer Tables)
// 0xFFFFFF7F_BFDFE000 - 0xFFFFFF7F_BFDFEFFF: Page Mapping Level 4 (PML4)
// 0xFFFFFF80_00000000 - 0xFFFFFFFF_7FFFFFFF: --- Free ---
// 0xFFFFFFFF_80000000 - 0xFFFFFFFF_8FFFFFFF: Kernel
// 0xFFFFFFFF_90000000 - 0xFFFFFFFF_FFFFFFFF: Jit code and data
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Direct map offset, static in memory, no KASLR please
 */
#define DIRECT_MAP_OFFSET   0xFFFF800000000000ULL

/**
 * The bottom of the stack allocator
 *
 * Each thread gets its own 8MB area, where we have 6MB of stack
 * and 2MB of guard "page", we can adjust these numbers as we see fit
 * This should be able to contain 1mb struct allocated on the stack
 * by managed code without any risk of overflowing and going to the
 * next stack
 */
#define STACKS_ADDR         (0xFFFF8F0000000000ULL)

/**
 * The threads range, this is where the thread structs are allocated, we have
 * 8MB per thread in theory, we won't practically use all of it most likely
 */
#define THREADS_ADDR        (0xFFFFA00000000000ULL)
#define THREADS_ADDR_END    (0xFFFFA07FFFFFFFFFULL)

/**
 * This is where the jit code and data lives
 */
#define JIT_ADDR            (0xFFFFFFFF90000000ULL)

/**
 * Convert direct map pointers as required
 */
#define PHYS_TO_DIRECT(x) (void*)((uintptr_t)(x) + DIRECT_MAP_OFFSET)
#define DIRECT_TO_PHYS(x) (uintptr_t)((uintptr_t)(x) - DIRECT_MAP_OFFSET)

// page size is 4k
#define PAGE_SIZE   SIZE_4KB
#define PAGE_MASK   0xFFF
#define PAGE_SHIFT  12

#define SIZE_TO_PAGES(size)   (((size) >> PAGE_SHIFT) + (((size) & PAGE_MASK) ? 1 : 0))
#define PAGES_TO_SIZE(pages)  ((pages) << PAGE_SHIFT)
