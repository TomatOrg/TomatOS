#include "phys.h"

#include <limine_requests.h>

#include "lib/string.h"
#include "lib/except.h"
#include "lib/list.h"
#include "virt.h"
#include "memory.h"
#include "sync/spinlock.h"
#include "limine.h"
#include "thread/pcpu.h"

static const char* m_limine_memmap_type_str[] = {
    [LIMINE_MEMMAP_USABLE] = "Usable",
    [LIMINE_MEMMAP_RESERVED] = "Reserved",
    [LIMINE_MEMMAP_ACPI_RECLAIMABLE] = "ACPI Reclaimable",
    [LIMINE_MEMMAP_ACPI_NVS] = "ACPI NVS",
    [LIMINE_MEMMAP_BAD_MEMORY] = "Bad Memory",
    [LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE] = "Bootloader Reclaimable",
    [LIMINE_MEMMAP_EXECUTABLE_AND_MODULES] = "Executable and Modules",
    [LIMINE_MEMMAP_FRAMEBUFFER] = "Framebuffer",
    [LIMINE_MEMMAP_ACPI_TABLES] = "ACPI Tables",
};

/**
 * We have a total of 16 buddy levels:
 * 0 - 16 bytes
 * 1 - 32 bytes
 * 2 - 64 bytes
 * 3 - 128 bytes
 * 4 - 256 bytes
 * 5 - 512 bytes
 * 6 - 1kb
 * 7 - 2kb
 * 8 - 4kb
 * 9 - 8kb
 * 10 - 16kb
 * 11 - 32kb
 * 12 - 64kb
 * 13 - 128kb
 * 14 - 256kb
 * 15 - 512kb
 * 16 - 1mb
 * 17 - 2mb
 * 18 - 4mb
 * 19 - 8mb
 * 20 - 16mb
 * 21 - 32mb
 * 22 - 64mb
 * 23 - 128mb
 */
#define BUDDY_LEVEL_COUNT   24

#define BUDDY_FIRST_LEVEL   4

/**
 * The max level size
 */
#define BUDDY_TOP_LEVEL_SIZE    (1 << ((BUDDY_LEVEL_COUNT - 1) + BUDDY_FIRST_LEVEL))

typedef union page_metadata {
    struct {
        // the level of the buddy
        uint8_t level : 5;

        // is this buddy free
        uint8_t free : 1;

        uint8_t : 2;
    };

    // the raw entry
    uint8_t raw;
} page_metadata_t;
STATIC_ASSERT(sizeof(page_metadata_t) == sizeof(uint8_t));

typedef struct memory_region {
    // the amount of pages in the region
    size_t page_count;

    // the direct map base of the region
    void* base;

    // the free list of pages in the region
    // for each buddy
    list_t free_list[BUDDY_LEVEL_COUNT];

    // the medata of the region
    page_metadata_t* metadata;
} memory_region_t;

static inline page_metadata_t* page_metadata(memory_region_t* region, void* addr) {
    // address is before the region, so not valid
    if (addr < region->base) {
        return NULL;
    }

    // get the index
    size_t index = (addr - region->base) / PAGE_SIZE;

    // address is after the region, not valid
    if (index >= region->page_count) {
        return NULL;
    }

    // return the metadata entry
    return &region->metadata[index];
}

/**
 * The memory regions we have
 */
static memory_region_t* m_memory_regions;

/**
 * The amount of memory regions we have
 */
static size_t m_memory_region_count;

/**
 * spinlock to protect against the allocator accesses
 */
static irq_spinlock_t m_memory_region_lock = IRQ_SPINLOCK_INIT;

/**
 * Find a region from its pointer
 */
static memory_region_t* find_region(void* ptr) {
    // just can linearly, we expect no more than 10 entries anyways
    for (int i = 0; i < m_memory_region_count; i++) {
        memory_region_t* region = &m_memory_regions[i];
        if (region->base <= ptr && ptr < region->base + region->page_count * PAGE_SIZE) {
            return region;
        }
    }
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Low level allocator management
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


static int get_level_by_size(uint32_t size) {
    // allocation is too big, return invalid
    if (size > BUDDY_TOP_LEVEL_SIZE) {
        return -1;
    }

    // allocation is too small, round to 4kb
    if (size < 16) {
        size = 16;
    }

    // align up to next power of two
    size = 1 << (32 - __builtin_clz(size - 1));

    // and now calculate the log2
    int level = 32 - __builtin_clz(size) - 1;

    // ignore the first 12 levels because they are less than 4kb
    return level - BUDDY_FIRST_LEVEL;
}

static void* allocate_from_level(memory_region_t* region, int level) {
    int block_at_level;
    list_entry_t* block = NULL;

    // -1 means that we wanted to allocate too much memory
    if (level == -1) {
        return NULL;
    }

    ASSERT(level < BUDDY_LEVEL_COUNT);

    // search from the level a block to allocate, and go
    // up if the list is empty
    for (block_at_level = level; block_at_level < BUDDY_LEVEL_COUNT; block_at_level++) {
        list_t* freelist = &region->free_list[block_at_level];
        if (list_is_empty(freelist)) {
            continue;
        }

        // the next is the new allocation
        block = freelist->next;

        // and we can remove it
        list_del(block);
        break;
    }

    // no more space
    if (block == NULL) {
        return NULL;
    }

    // split the block until we reach
    // the requested level
    while (block_at_level > level) {
        // calculate the size
        size_t block_size = 1 << (block_at_level + BUDDY_FIRST_LEVEL);
        block_at_level--;

        // split it to two, adding the higher half to the level below us
        list_entry_t* upper = (list_entry_t*)(((uintptr_t)block) + block_size / 2);
        list_add(&region->free_list[block_at_level], upper);

        // mark the upper block as the new level it is at
        page_metadata_t* metadata = page_metadata(region, upper);
        ASSERT(metadata != NULL);
        metadata->level = block_at_level;
        metadata->free = true;
    }

    // mark our block as allocated by setting the available to zero
    page_metadata_t* metadata = page_metadata(region, block);
    ASSERT(metadata != NULL);
    metadata->level = level;
    metadata->free = false;

    // return the block we found
    return block;
}

static void free_at_level(memory_region_t* region, void* ptr, int level) {
    ASSERT(level < BUDDY_LEVEL_COUNT);
    while (level < (BUDDY_LEVEL_COUNT - 1)) {
        // get the current block size
        int block_size = 1 << (level + BUDDY_FIRST_LEVEL);

        // 1 if the neighbor is above us, -1 if it is below us
        int neighbor = ((uintptr_t)ptr & ((block_size * 2) - 1)) == 0 ? 1 : -1;

        list_entry_t* neighbor_entry = ptr + neighbor * block_size;
        page_metadata_t* neighbor_pt = page_metadata(region, neighbor_entry);
        if (neighbor_pt == NULL) {
            break;
        }

        // check that neighbor is not allocated
        if (!neighbor_pt->free) {
            break;
        }

        // check if the neighbor is at the same level, if it
        // is then merge with it
        int neighbor_level = neighbor_pt->level;
        if (neighbor_level != level) {
            break;
        }

        // remove from the current level
        list_del(neighbor_entry);

        // get the new pointer, if the neighbor was below then
        // get the neighbor entry
        if (neighbor == -1) {
            ptr -= block_size;
        }

        // next level please
        level++;
    }

    // set the block size now, to indicate this entire range is free for this level
    page_metadata_t* metadata = page_metadata(region, ptr);
    ASSERT(metadata != NULL);
    metadata->level = level;
    metadata->free = true;

    // we now know the correct level, add the ptr to it
    list_entry_t* ptr_entry = ptr;
    list_add(&region->free_list[level], ptr_entry);
}

static void add_memory_to_region(memory_region_t* region, void* base, size_t page_count) {
    int page_level = get_level_by_size(SIZE_4KB);

    // and add the rest
    while (((uintptr_t)base % BUDDY_TOP_LEVEL_SIZE) != 0 && page_count != 0) {
        free_at_level(region, base, page_level);
        base += SIZE_4KB;
        page_count--;
    }

    while (page_count >= (BUDDY_TOP_LEVEL_SIZE / SIZE_4KB)) {
        free_at_level(region, base, BUDDY_LEVEL_COUNT - 1);
        base += BUDDY_TOP_LEVEL_SIZE;
        page_count -= (BUDDY_TOP_LEVEL_SIZE / SIZE_4KB);
    }

    while (page_count != 0) {
        free_at_level(region, base, page_level);
        base += SIZE_4KB;
        page_count--;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Top level mangement
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Add memory from the memory map with the given
 * type to the allocator
 */
static err_t add_memory(uint64_t type) {
    err_t err = NO_ERROR;

    size_t pages_added = 0;

    struct limine_memmap_response* response = g_limine_memmap_request.response;
    for (int i = 0; i < response->entry_count; i++) {
        struct limine_memmap_entry* entry = response->entries[i];
        if (entry->type == type) {
            void* base = PHYS_TO_DIRECT(entry->base);
            size_t page_count = entry->length / PAGE_SIZE;

            memory_region_t* region = find_region(base);
            CHECK(region != NULL);
            add_memory_to_region(region, base, page_count);
            pages_added += page_count;
        }
    }

    TRACE("memory: Added a total of %lu pages", pages_added);

cleanup:
    return err;
}

static err_t create_memory_regions(void) {
    err_t err = NO_ERROR;

    struct limine_memmap_response* response = g_limine_memmap_request.response;
    CHECK(response->entry_count > 0);

    //
    // Start by counting the amount of regions we have and the amount of
    // physical memory we have, this will tell us how much memory we will
    // need in total for the entire allocation
    //
    uintptr_t total_usable_pages = 0;
    uintptr_t region_end = -1;
    struct limine_memmap_entry* largest_region = NULL;
    for (int i = 0; i < response->entry_count; i++) {
        struct limine_memmap_entry* entry = response->entries[i];

        if (entry->type < ARRAY_LENGTH(m_limine_memmap_type_str)) {
            TRACE("memory: %016lx-%016lx: %s", entry->base, entry->base + entry->length, m_limine_memmap_type_str[entry->type]);
        } else {
            TRACE("memory: %016lx-%016lx: <unknown type %lu>", entry->base, entry->base + entry->length, entry->type);
        }

        // check if this is usable and if so if a region is needed
        switch (entry->type) {
            case LIMINE_MEMMAP_USABLE:
                // check if this is larger
                if (largest_region == NULL || entry->length > largest_region->length) {
                    largest_region = entry;
                }

            case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
            case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES:
            case LIMINE_MEMMAP_ACPI_RECLAIMABLE: {
                total_usable_pages += (entry->length / PAGE_SIZE);

                // if the last region end and the new base are not the same
                // we have a new region
                if (region_end != entry->base) {
                    m_memory_region_count++;
                }

                region_end = entry->base + entry->length;

            } break;

            default:
                // unknown entry breaks the chain
                region_end = -1;
                break;
        }
    }
    CHECK(m_memory_region_count > 0);

    // allocate from the largest region the required overhead
    size_t overhead = m_memory_region_count * sizeof(memory_region_t) + total_usable_pages;
    overhead = ALIGN_UP(overhead, PAGE_SIZE);
    CHECK_ERROR(largest_region->length > overhead, ERROR_OUT_OF_MEMORY);
    void* metadata = PHYS_TO_DIRECT(largest_region->base + (largest_region->length - overhead));
    memset(metadata, 0, overhead);

    // first set the regions array
    m_memory_regions = metadata;
    metadata += m_memory_region_count * sizeof(memory_region_t);
    size_t metadata_left = total_usable_pages;

    // initialize the buddies of all the regions
    for (int i = 0; i < m_memory_region_count; i++) {
        for (int j = 0; j < ARRAY_LENGTH(m_memory_regions[i].free_list); j++) {
            list_init(&m_memory_regions[i].free_list[j]);
        }
    }

    // and now we need to set the regions
    int region_i = -1;
    region_end = -1;
    for (int i = 0; i < response->entry_count; i++) {
        struct limine_memmap_entry* entry = response->entries[i];

        // check if this is usable and if so if a region is needed
        switch (entry->type) {
            case LIMINE_MEMMAP_USABLE:
            case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
            case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES:
            case LIMINE_MEMMAP_ACPI_RECLAIMABLE: {
                size_t page_count = entry->length / PAGE_SIZE;

                // make sure we have space for this
                CHECK(metadata_left >= page_count);
                metadata_left -= page_count;

                // if the last region end and the new base are not the same
                // we have a new region
                if (region_end != entry->base) {
                    region_i++;
                    CHECK(region_i < m_memory_region_count);
                    m_memory_regions[region_i].base = PHYS_TO_DIRECT(entry->base);
                    m_memory_regions[region_i].metadata = metadata;
                }

                // add the page count to the current region
                m_memory_regions[region_i].page_count += page_count;

                // set the region end
                region_end = entry->base + entry->length;

                // metadata entries were allocated no
                // matter which path we took
                metadata += page_count;
            } break;

            default:
                // unknown entry breaks the chain
                region_end = -1;
                break;
        }
    }

    //
    // now that all the regions are valid, lets free
    // some memory into the allocator, remember that we need
    // to remove the overhead from the largest region so
    // we won't treat it as usable
    //
    largest_region->length -= overhead;
    RETHROW(add_memory(LIMINE_MEMMAP_USABLE));
    largest_region->length += overhead;

cleanup:
    return err;
}

err_t init_phys() {
    err_t err = NO_ERROR;

    // we need this
    CHECK(g_limine_memmap_request.response != NULL);

    // add the usable entries
    TRACE("memory: Adding physical memory");
    RETHROW(create_memory_regions());

cleanup:
    return err;
}

err_t init_phys_mappings() {
    err_t err = NO_ERROR;

    struct limine_memmap_response* response = g_limine_memmap_request.response;

    // and now add all the different entries
    TRACE("memory: Mapping physical memory");
    for (int i = 0; i < response->entry_count; i++) {
        struct limine_memmap_entry* entry = response->entries[i];

        map_flags_t flags = 0;
        switch (entry->type) {
            // read-write memory
            case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
            case LIMINE_MEMMAP_ACPI_NVS:
            case LIMINE_MEMMAP_RESERVED:
            case LIMINE_MEMMAP_USABLE:
            case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
                flags = MAP_PERM_W;
                break;

            // readonly mappings
            case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES:
            case LIMINE_MEMMAP_ACPI_TABLES:
                break;

            // map as Write Combining
            case LIMINE_MEMMAP_FRAMEBUFFER:
                flags = MAP_PERM_W;
                break;

            // don't map
            case LIMINE_MEMMAP_BAD_MEMORY:
                continue;

            // invalid entries
            default:
                WARN("Unknown memory type %lu", entry->type);
                continue;
        }

        // if this is usable add to page list
        uintptr_t phys_base = ALIGN_DOWN(entry->base, PAGE_SIZE);
        void* virt_base = PHYS_TO_DIRECT(phys_base);
        void* virt_end = PHYS_TO_DIRECT(ALIGN_UP(entry->base + entry->length, PAGE_SIZE));
        RETHROW(virt_map_range(phys_base, (uintptr_t)virt_base, (virt_end - virt_base) / PAGE_SIZE, flags));
    }

cleanup:
    return err;
}

void phys_reclaim_bootloader() {
    TRACE("memory: Adding bootloader reclaimable memory");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Page allocation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void* internal_phys_alloc(int level) {
    void* ptr = NULL;
    for (int i = 0; i < m_memory_region_count; i++) {
        ptr = allocate_from_level(&m_memory_regions[i], level);
        if (ptr != NULL) {
            break;
        }
    }
    return ptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IRQ allocation
//
// This is meant for cases where a palloc caused a PF for demand paging (can happen with stack growing)
// which means that we need to properly be able to allocate memory for such a case without trying to take
// the allocator lock again
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * The lock which taken the cpu
 */
static int m_lock_cpu = -1;

/**
 * A reserved page used for
 */
static CPU_LOCAL void* m_reserved_page;

/**
 * There is one case where an on-demand stack expansion may
 * happen inside the physical allocator, for that case we need
 * allocate a page for it while already locked, we do it with the
 * reserved page, and assume that it will always be enough
 */
static void* irq_alloc(size_t size) {
    if (m_lock_cpu == get_cpu_id() && size == PAGE_SIZE) {
        ASSERT(m_reserved_page != NULL);
        void* ptr = m_reserved_page;
        m_reserved_page = NULL;
        return ptr;
    }

    return NULL;
}

/**
 * Fill the reserved pool with pages so IRQ context can use them
 */
static void fill_irq_alloc() {
    if (m_reserved_page == NULL) {
        void* page = internal_phys_alloc(get_level_by_size(PAGE_SIZE));
        if (page == NULL) {
            WARN("phys: out of memory to fill the reserved pages pool");
            return;
        }
        m_reserved_page = page;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void* phys_alloc(size_t size) {
    // attempt to allocate using the irq alloc
    // if returns NULL then attempt to use the
    // normal route
    void* ptr = irq_alloc(size);
    if (ptr != NULL) {
        return ptr;
    }

    // calculate the size
    int level = get_level_by_size(size);
    if (level == -1) {
        return NULL;
    }

    // lock and record that we are the locker
    bool irq_state = irq_spinlock_acquire(&m_memory_region_lock);
    m_lock_cpu = get_cpu_id();

    // perform the allocation safely
    ptr = internal_phys_alloc(level);

    // fill the IRQ allocation if need be
    fill_irq_alloc();

    // remove the lock
    m_lock_cpu = -1;
    irq_spinlock_release(&m_memory_region_lock, irq_state);

    return ptr;
}


void* early_phys_alloc(size_t size) {
    int level = get_level_by_size(size);
    if (level == -1) {
        return NULL;
    }

    bool irq_state = irq_spinlock_acquire(&m_memory_region_lock);
    void* ptr = internal_phys_alloc(level);
    irq_spinlock_release(&m_memory_region_lock, irq_state);

    return ptr;
}

void phys_free(void* ptr) {
    if (ptr == NULL) {
        return;
    }

    bool irq_state = irq_spinlock_acquire(&m_memory_region_lock);

    // get the region
    memory_region_t* region = find_region(ptr);
    ASSERT(region != NULL);

    // get and verify the metadata
    page_metadata_t* metadata = page_metadata(region, ptr);
    int level = metadata->level;
    ASSERT(((uintptr_t)ptr & ((1 << (level + BUDDY_FIRST_LEVEL)) - 1)) == 0);

    // and now actually free it
    free_at_level(region, ptr, level);

    irq_spinlock_release(&m_memory_region_lock, irq_state);
}

void init_phys_per_cpu() {
    // make sure we have an available reserved page
    bool irq_state = irq_spinlock_acquire(&m_memory_region_lock);
    fill_irq_alloc();
    irq_spinlock_release(&m_memory_region_lock, irq_state);
}
