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
    uint64_t available : 11;
    uint64_t no_execute : 1;
} PACKED page_entry_t;
STATIC_ASSERT(sizeof(page_entry_t) == sizeof(uint64_t));

typedef signed long long pml_index_t;

#define PAGE_TABLE_PML1     ((page_entry_t*)0xFFFFFF0000000000ull)
#define PAGE_TABLE_PML2     ((page_entry_t*)0xFFFFFF7F80000000ull)
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
 * Setup a single level of the page table, this will allocate the page
 * if needed
 *
 * @param pml       [IN] The PML virtual base
 * @param pml       [IN] The next PML virtual base
 * @param index     [IN] The index of the page in the level
 */
bool vmm_setup_level(page_entry_t* pml, page_entry_t* next_pml, size_t index);

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
 * Checks if the given address is mapped
 */
bool vmm_is_mapped(uintptr_t ptr);

/**
 * The phys fault handler for the system, the VMM will check if the request should
 * do any COW or on demand mapping and handle it
 *
 * @param fault_address     [IN] The virtual address
 * @param write             [IN] Was this a read or write
 * @param present           [IN] Is the phys present or is it unpresent
 */
err_t vmm_page_fault_handler(uintptr_t fault_address, bool write, bool present);
