#include "phys.h"

#include "lib/string.h"
#include "lib/except.h"
#include "lib/list.h"
#include "virt.h"
#include "memory.h"
#include "sync/spinlock.h"
#include "limine.h"
#include "thread/pcpu.h"

LIMINE_REQUEST struct limine_memmap_request g_limine_memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST
};

static const char* m_limine_memmap_type_str[] = {
    [LIMINE_MEMMAP_USABLE] = "Usable",
    [LIMINE_MEMMAP_RESERVED] = "Reserved",
    [LIMINE_MEMMAP_ACPI_RECLAIMABLE] = "ACPI Reclaimable",
    [LIMINE_MEMMAP_ACPI_NVS] = "ACPI NVS",
    [LIMINE_MEMMAP_BAD_MEMORY] = "Bad Memory",
    [LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE] = "Bootloader Reclaimable",
    [LIMINE_MEMMAP_KERNEL_AND_MODULES] = "Kernel and Modules",
    [LIMINE_MEMMAP_FRAMEBUFFER] = "Framebuffer",
};

/**
 * We have a total of 16 buddy levels:
 * 0 - 4kb
 * 1 - 8kb
 * 2 - 16kb
 * 3 - 32kb
 * 4 - 64kb
 * 5 - 128kb
 * 6 - 256kb
 * 7 - 512kb
 * 8 - 1mb
 * 9 - 2mb
 * 10 - 4mb
 * 11 - 8mb
 * 12 - 16mb
 * 13 - 32mb
 * 14 - 64mb
 * 15 - 128mb
 */
#define BUDDY_LEVEL_COUNT 16

/**
 * The max level size
 */
#define BUDDY_TOP_LEVEL_SIZE    (1 << ((BUDDY_LEVEL_COUNT - 1) + 12))

typedef union page_metadata {
    struct {
        // the level of the buddy
        uint8_t level : 4;

        // is this buddy free
        uint8_t free : 1;

        // was this entry properly added to the allocator
        uint8_t available : 1;

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
static spinlock_t m_memory_region_lock = INIT_SPINLOCK();

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

        for (int i = sizeof(list_entry_t); i < 1 << (block_at_level + 12); i++) {
            uint8_t* ptr = (uint8_t*)block;
            ASSERT(ptr[i] == 0xAA);
        }

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
        size_t block_size = 1 << (block_at_level + 12);
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
        int block_size = 1 << (level + 12);

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

        // must be an available entry in order to merge
        ASSERT(neighbor_pt->available);

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
    metadata->available = true;

    // we now know the correct level, add the ptr to it
    memset(ptr, 0xAA, 1 << (level + 12));
    list_entry_t* ptr_entry = ptr;
    list_add(&region->free_list[level], ptr_entry);
}

static void add_memory_to_region(memory_region_t* region, void* base, size_t page_count) {
    // and add the rest
    while (page_count != 0) {
        free_at_level(region, base, 0);
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

    LOG_INFO("memory: Added a total of %d pages", pages_added);

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
            LOG_INFO("memory: %p-%p: %s", entry->base, entry->base + entry->length, m_limine_memmap_type_str[entry->type]);
        } else {
            LOG_INFO("memory: %p-%p: <unknown type %d>", entry->base, entry->base + entry->length, entry->type);
        }

        // check if this is usable and if so if a region is needed
        switch (entry->type) {
            case LIMINE_MEMMAP_USABLE:
                // check if this is larger
                if (largest_region == NULL || entry->length > largest_region->length) {
                    largest_region = entry;
                }

            case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
            case LIMINE_MEMMAP_KERNEL_AND_MODULES:
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
            case LIMINE_MEMMAP_KERNEL_AND_MODULES:
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
    LOG_INFO("memory: Adding physical memory");
    RETHROW(create_memory_regions());

cleanup:
    return err;
}

err_t init_phys_mappings() {
    err_t err = NO_ERROR;

    struct limine_memmap_response* response = g_limine_memmap_request.response;

    // and now add all the different entries
    LOG_INFO("memory: Mapping physical memory");
    for (int i = 0; i < response->entry_count; i++) {
        struct limine_memmap_entry* entry = response->entries[i];

        map_flags_t flags = 0;
        switch (entry->type) {
            // read-write memory
            case LIMINE_MEMMAP_USABLE:
            case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
                flags = MAP_PERM_W;
                break;

            // readonly mappings
            case LIMINE_MEMMAP_RESERVED:
            case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
            case LIMINE_MEMMAP_ACPI_NVS:
            case LIMINE_MEMMAP_KERNEL_AND_MODULES:
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
                LOG_WARN("Unknown memory type %d", entry->type);
                continue;
        }

        // if this is usable add to page list
        uintptr_t phys_base = ALIGN_DOWN(entry->base, PAGE_SIZE);
        void* virt_base = PHYS_TO_DIRECT(phys_base);
        void* virt_end = PHYS_TO_DIRECT(ALIGN_UP(entry->base + entry->length, PAGE_SIZE));
        RETHROW(virt_map_range(phys_base, (uintptr_t)virt_base, (virt_end - virt_base) / PAGE_SIZE, flags));
}

    // Map the APIC as well
    RETHROW(virt_map_page(0xFEE00000, (uintptr_t)PHYS_TO_DIRECT(0xFEE00000), MAP_PERM_W));

cleanup:
    return err;
}

void phys_reclaim_bootloader() {
    LOG_INFO("memory: Adding bootloader reclaimable memory");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Page allocation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * A per CPU stack for allocating
 */
static CPU_LOCAL void* m_alloc_stack = NULL;

static int get_level_by_size(uint32_t size) {
    // allocation is too big, return invalid
    if (size > SIZE_2MB) {
        return -1;
    }

    // allocation is too small, round to 4kb
    if (size < SIZE_4KB) {
        size = SIZE_4KB;
    }

    // align up to next power of two
    size = 1 << (32 - __builtin_clz(size - 1));

    // and now calculate the log2
    int level = 32 - __builtin_clz(size) - 1;

    // ignore the first 12 levels because they are less than 4kb
    return level - 12;
}

/**
 * Performs the actual allocation, this uses a per-cpu stack that is meant
 * specifically for allocation, this ensures that we won't run into a case
 * where an allocation is needed to fill the demand paging of the stack while
 * already allocating
 */
__attribute__((used))
static void* internal_phys_alloc(int level) {
    spinlock_lock(&m_memory_region_lock);

    void* ptr = NULL;
    for (int i = 0; i < m_memory_region_count; i++) {
        ptr = allocate_from_level(&m_memory_regions[i], level);
        if (ptr != NULL) {
            break;
        }
    }

    spinlock_unlock(&m_memory_region_lock);

    return ptr;
}

__attribute__((naked))
static void* call_internal_phys_alloc(int size, void* stack) {
    asm(
        "mov %rsp, %rbx\n"
        "mov %rsi, %rsp\n"
        "call internal_phys_alloc\n"
        "mov %rbx, %rsp\n"
        "ret"
    );
}

/**
 * Same as the internal_phys_alloc, but for freeing
 */
__attribute__((used))
static void internal_phys_free(void* ptr) {
    spinlock_lock(&m_memory_region_lock);

    // get the region
    memory_region_t* region = find_region(ptr);
    ASSERT(region != NULL);

    // get and verify the metadata
    page_metadata_t* metadata = page_metadata(region, ptr);
    int level = metadata->level;
    ASSERT(((uintptr_t)ptr & ((1 << (level + 12)) - 1)) == 0);

    // and now actually free it
    free_at_level(region, ptr, level);

    spinlock_unlock(&m_memory_region_lock);
}

__attribute__((naked))
static void call_internal_phys_free(void* ptr, void* stack) {
    asm(
        "mov %rsp, %rbx\n"
        "mov %rsi, %rsp\n"
        "call internal_phys_free\n"
        "mov %rbx, %rsp\n"
        "ret"
    );
}

void* phys_alloc(size_t size) {
    int level = get_level_by_size(size);
    if (level == -1) {
        return NULL;
    }

    bool irq = irq_save();
    void* ptr = call_internal_phys_alloc(level, m_alloc_stack);
    irq_restore(irq);

    return ptr;
}


void* early_phys_alloc(size_t size) {
    int level = get_level_by_size(size);
    if (level == -1) {
        return NULL;
    }

    bool irq = irq_save();
    void* ptr = internal_phys_alloc(level);
    irq_restore(irq);

    return ptr;
}

void phys_free(void* ptr) {
    if (ptr == NULL) {
        return;
    }

    bool irq = irq_save();
    call_internal_phys_free(ptr, m_alloc_stack);
    irq_restore(irq);
}

err_t init_phys_per_cpu() {
    err_t err = NO_ERROR;

    // allocate the alloc stack without using it
    void* ptr = internal_phys_alloc(0);
    CHECK_ERROR(ptr != NULL, ERROR_OUT_OF_MEMORY);

    m_alloc_stack = ptr + PAGE_SIZE;

cleanup:
    return err;
}

void phys_dump_buddy() {
    LOG_INFO("memory regions");

    for (int i = 0; i < m_memory_region_count; i++) {
        memory_region_t* region = &m_memory_regions[i];

        LOG_INFO("\t0x%08x-0x%08x", region->base, region->base + region->page_count * PAGE_SIZE);
        size_t until_next_free = 0;
        for (size_t i = 0; i < region->page_count; i++) {
            page_metadata_t* metadata = &region->metadata[i];
            if (until_next_free == 0) {
                if (metadata->available) {
                    LOG_INFO("\t\t0x%08x-0x%08x (%d): %s",
                        region->base + i * PAGE_SIZE, region->base + i * PAGE_SIZE + (1 << (metadata->level + 12)),
                        metadata->level, metadata->free ? "free" : "alloc");
                } else {
                    if (metadata->raw != 0) {
                        LOG_ERROR("\t\t%d: <METADATA CORRUPTION> (under unavailable range) %02x", i, metadata->raw);
                    }
                }
                until_next_free = (1 << metadata->level) - 1;
            } else {
                until_next_free--;
            }
        }
    }
}
