#pragma once

#include <util/except.h>
#include <util/defs.h>
#include <stdbool.h>

typedef struct page_entry {
    uint64_t present : 1;
    uint64_t writeable : 1;
    uint64_t user_accessible : 1;
    uint64_t write_through : 1;
    uint64_t no_cache : 1;
    uint64_t accessed : 1;
    uint64_t dirty : 1;
    uint64_t huge_page : 1;
    uint64_t global : 1;
    uint64_t available_low : 3;
    uint64_t frame : 40;
    uint64_t available_high : 11;
    uint64_t no_execute : 1;
} PACKED page_entry_t;
STATIC_ASSERT(sizeof(page_entry_t) == sizeof(uint64_t));

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
    MAP_UNMAP_DIRECT = (1 << 2)
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
 * Unmap the given virtual address.
 *
 * @remark
 * If the page you are trying to unmap is not already mapped a page fault could occur
 *
 * @param va            [IN] The virtual address
 * @param page_count    [IN] The amount of pages to unmap
 * @param phys          [IN, OPTIONAL] Will contain the physical addresses that were unmapped
 */
void vmm_unmap(void* va, size_t page_count, uintptr_t* phys);

/**
 * The phys fault handler for the system, the VMM will check if the request should
 * do any COW or on demand mapping and handle it
 *
 * @param fault_address     [IN] The virtual address
 * @param write             [IN] Was this a read or write
 * @param present           [IN] Is the phys present or is it unpresent
 */
err_t vmm_page_fault_handler(uintptr_t fault_address, bool write, bool present);
