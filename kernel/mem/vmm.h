#pragma once

#include <util/except.h>
#include <util/defs.h>
#include <stdbool.h>

typedef struct page_entry_4kb {
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

    // -- available : 11 --
    // see the phys.c for more info
    uint64_t buddy_level : 4; // the level of the buddy
    uint64_t buddy_alloc : 1; // is this buddy allocated
    uint64_t _available1 : 6;

    uint64_t no_execute : 1;
} PACKED page_entry_4kb_t;
STATIC_ASSERT(sizeof(page_entry_4kb_t) == sizeof(uint64_t));

typedef struct page_entry_2mb {
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

    // -- available : 11 --
    // see the phys.c for more info
    uint64_t buddy_level : 4;
    uint64_t buddy_alloc : 1;
    uint64_t _available1 : 6;

    uint64_t no_execute : 1;
} PACKED page_entry_2mb_t;
STATIC_ASSERT(sizeof(page_entry_2mb_t) == sizeof(uint64_t));

typedef struct page_entry {
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

    // -- available : 11 --
    // see the phys.c for more info
    uint64_t buddy_level : 4;
    uint64_t buddy_alloc : 1;
    uint64_t _available3 : 6;

    uint64_t no_execute : 1;
} PACKED page_entry_t;
STATIC_ASSERT(sizeof(page_entry_2mb_t) == sizeof(uint64_t));

typedef enum caching_mode {
    CACHE_WRITE_BACK = 0,
    CACHE_WRITE_THROUGH,
    CACHE_UNCACHED,
    CACHE_UNCACHEABLE,
    CACHE_WRITE_BACK_,
    CACHE_WRITE_THROUGH_,
    CACHE_UNCACHED_,
    CACHE_WRITE_COMBINING,
} caching_mode_t;

typedef signed long long pml_index_t;

#define PAGE_TABLE_PML1     ((page_entry_4kb_t*)0xFFFFFF0000000000ull)
#define PAGE_TABLE_PML2     ((page_entry_2mb_t*)0xFFFFFF7F80000000ull)
#define PAGE_TABLE_PML3     ((page_entry_t*)0xFFFFFF7FBFC00000ull)
#define PAGE_TABLE_PML4     ((page_entry_t*)0xFFFFFF7FBFDFE000ull)

#define PML4_INDEX(va)      ((pml_index_t)(((uintptr_t)(va) >> 39) & 0x1FFull))
#define PML3_INDEX(va)      ((pml_index_t)(((uintptr_t)(va) >> 30) & 0x3FFFFull))
#define PML2_INDEX(va)      ((pml_index_t)(((uintptr_t)(va) >> 21) & 0x7FFFFFFull))
#define PML1_INDEX(va)      ((pml_index_t)(((uintptr_t)(va) >> 12) & 0xFFFFFFFFFull))

#define PML4_BASE(idx)      ((uintptr_t)(SIGN_EXTEND((pml_index_t)(idx) << 39, 48)))
#define PML3_BASE(idx)      ((uintptr_t)(SIGN_EXTEND((pml_index_t)(idx) << 30, 48)))
#define PML2_BASE(idx)      ((uintptr_t)(SIGN_EXTEND((pml_index_t)(idx) << 21, 48)))
#define PML1_BASE(idx)      ((uintptr_t)(SIGN_EXTEND((pml_index_t)(idx) << 12, 48)))

/**
 * Represents an invalid physical address for related functions
 */
#define INVALID_PHYS_ADDR ((uintptr_t)-1)

/**
 * Initialize the vmm itself
 */
err_t init_vmm();

/**
 * Tells the VMM we are ready to switch to the
 * phys allocator
 */
void vmm_switch_allocator();

/**
 * Initialize the vmm per cpu
 */
void init_vmm_per_cpu();

/**
 * Unmap a single page from the direct map, don't page fault
 * if the page is not already mapped.
 *
 * @param pa    [IN] the physical page to unmap
 */
void vmm_unmap_direct_page(uintptr_t pa);

typedef enum map_perm {
    /**
     * Map the page as writable
     */
    MAP_WRITE = (1 << 0),

    /*
     * Map the page as executable
     */
    MAP_EXEC = (1 << 1),

    /*
     * While mapping the physical memory remove it from the direct
     * memory map.
     */
    MAP_UNMAP_DIRECT = (1 << 2),

    /*
     * Map the page as write-combining.
     */
    MAP_WC = (1 << 3),

    /**
     * Map a large page, instead of a small one
     */
    MAP_LARGE = (1 << 4),
} map_perm_t;

/**
 * Map the given physical phys into virtual memory.
 *
 * This will override any already mapped pages
 *
 * @param pa            [IN] The physical address
 * @param va            [IN] The virtual address
 * @param page_count    [IN] The amount of pages to map
 * @param perms         [IN] The permissions to set
 */
err_t vmm_map(uintptr_t pa, void* va, size_t page_count, map_perm_t perms);

/**
 * This allows to set protections attributes
 *
 * @param va            [IN] The virtual address
 * @param page_count    [IN] The number of pages
 * @param perms         [IN] The new permissions
 */
err_t vmm_set_perms(void* va, size_t page_count, map_perm_t perms);

/**
 * Allocates and maps physical memory (could be non-contigious) to the given virtual
 * address range.
 *
 * If the page is already maps this will fail and will unmap and free all the allocated
 * pages until the point of the failure.
 *
 * @param va            [IN] The virtual address
 * @param page_count    [IN] The page count
 * @param perms         [IN] The permissions to set
 */
err_t vmm_alloc(void* va, size_t page_count, map_perm_t perms);

/**
 * Checks if the given address is mapped
 */
bool vmm_is_mapped(uintptr_t ptr, size_t size);

/**
 * The phys fault handler for the system, the VMM will check if the request should
 * do any COW or on demand mapping and handle it
 *
 * @param fault_address     [IN] The virtual address
 * @param write             [IN] Was this a read or write
 * @param present           [IN] Is the phys present or is it unpresent
 */
err_t vmm_page_fault_handler(uintptr_t fault_address, bool write, bool present, bool* handled);
