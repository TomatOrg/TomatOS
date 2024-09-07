#pragma once

#include <stdbool.h>

#include "lib/defs.h"
#include "lib/except.h"

typedef union page_entry_4kb {
    struct {
        uint64_t present : 1;
        uint64_t writeable : 1;
        uint64_t user_accessible : 1;
        uint64_t pat0 : 1;
        uint64_t pat1 : 1;
        uint64_t accessed : 1;
        uint64_t dirty : 1;
        uint64_t pat2 : 1;
        uint64_t global : 1;
        uint64_t _available : 3;
        uint64_t frame : 40;
        uint64_t _available1 : 11;
        uint64_t no_execute : 1;
    };
    uint64_t packed;
} PACKED page_entry_4kb_t;
STATIC_ASSERT(sizeof(page_entry_4kb_t) == sizeof(uint64_t));

typedef union page_entry_2mb {
    struct {
        uint64_t present : 1;
        uint64_t writeable : 1;
        uint64_t user_accessible : 1;
        uint64_t pat0 : 1;
        uint64_t pat1 : 1;
        uint64_t accessed : 1;
        uint64_t dirty : 1;
        uint64_t huge_page : 1;
        uint64_t global : 1;
        uint64_t _available : 3;
        uint64_t pat2 : 1;
        uint64_t _reserved : 8;
        uint64_t frame : 31;
        uint64_t _available3 : 11;
        uint64_t no_execute : 1;
    };
    uint64_t packed;
} PACKED page_entry_2mb_t;
STATIC_ASSERT(sizeof(page_entry_2mb_t) == sizeof(uint64_t));

typedef union page_entry {
    struct {
        uint64_t present : 1;
        uint64_t writeable : 1;
        uint64_t user_accessible : 1;
        uint64_t pwt : 1;
        uint64_t pcd : 1;
        uint64_t accessed : 1;
        uint64_t _available1 : 1;
        uint64_t huge_page : 1;
        uint64_t _available2 : 4;
        uint64_t frame : 40;
        uint64_t _available3 : 11;
        uint64_t no_execute : 1;
    };
    uint64_t packed;
} PACKED page_entry_t;
STATIC_ASSERT(sizeof(page_entry_2mb_t) == sizeof(uint64_t));

#define PML4_INDEX(va)      (((uintptr_t)(va) >> 39) & 0x1FFull)
#define PML3_INDEX(va)      (((uintptr_t)(va) >> 30) & 0x1FFull)
#define PML2_INDEX(va)      (((uintptr_t)(va) >> 21) & 0x1FFull)
#define PML1_INDEX(va)      (((uintptr_t)(va) >> 12) & 0x1FFull)

/**
 * Early init, before we have a physical memory allocator
 */
err_t init_virt_early();

/**
 * Normal init, setting up the page tables before we can switch to them
 */
err_t init_virt();

typedef enum map_flags {
    MAP_PERM_W = BIT0,
    MAP_PERM_X = BIT1,
} map_flags_t;

/**
 * Map a physical page to a virtual page, this is irq safe
 */
err_t virt_map_page(uint64_t phys, uintptr_t virt, map_flags_t flags);

err_t virt_map_range(uint64_t phys, uintptr_t virt, size_t page_count, map_flags_t flags);

/**
 * Allocate the given virtual range, allocating physical pages as required
 */
err_t virt_alloc_range(uintptr_t virt, size_t page_count);

/**
 * Remap the given memory range with the given protection flags
 */
err_t virt_remap_range(uintptr_t virt, size_t page_count, map_flags_t flags);

bool virt_is_mapped(uintptr_t virt);

/**
 * Switch to the kernel's page table
 */
void switch_page_table();

/**
 * Attempt to handle a page fault for lazy-memory allocation
 */
bool virt_handle_page_fault(uintptr_t addr);
