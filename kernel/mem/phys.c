
#include "lib/except.h"
#include "lib/list.h"
#include "virt.h"
#include "memory.h"
#include "sync/spinlock.h"
#include "limine.h"

static spinlock_t m_phys_lock;

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

static list_t m_page_freelist;

static size_t phys_add_memory(uint64_t type, bool log) {
    struct limine_memmap_response* response = g_limine_memmap_request.response;
    size_t total_added = 0;

    // and now add all the different entries
    for (int i = 0; i < response->entry_count; i++) {
        struct limine_memmap_entry* entry = response->entries[i];

        if (log) {
            if (entry->type < ARRAY_LENGTH(m_limine_memmap_type_str)) {
                LOG_INFO("memory: %p-%p: %s", entry->base, entry->base + entry->length, m_limine_memmap_type_str[entry->type]);
            } else {
                LOG_INFO("memory: %p-%p: <unknown type %d>", entry->base, entry->base + entry->length, entry->type);
            }
        }

        // if this is usable add to page list
        if (entry->type == type) {
            for (uintptr_t page = entry->base; page < entry->base + entry->length; page += SIZE_4KB) {
                list_add(&m_page_freelist, PHYS_TO_DIRECT(page));
                total_added++;
            }
        }
    }

    return total_added;
}

err_t init_phys() {
    err_t err = NO_ERROR;

    // we need this
    CHECK(g_limine_memmap_request.response != NULL);

    list_init(&m_page_freelist);

    // add the usable entries
    LOG_INFO("memory: Adding physical memory");
    size_t pages = phys_add_memory(LIMINE_MEMMAP_USABLE, true);
    LOG_INFO("memory: Added a total of %d pages", pages);

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
        RETHROW(virt_map_range(
                entry->base, (uintptr_t)PHYS_TO_DIRECT(entry->base),
                DIV_ROUND_UP(entry->length, SIZE_4KB),
                flags));
    }

    // Map the APIC as well
    RETHROW(virt_map_page(0xFEE00000, (uintptr_t)PHYS_TO_DIRECT(0xFEE00000), MAP_PERM_W));

cleanup:
    return err;
}

void phys_reclaim_bootloader() {
    LOG_INFO("memory: Adding bootloader reclaimable memory");
    size_t pages = phys_add_memory(LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE, false);
    LOG_INFO("memory: Added a total of %d pages", pages);
}

void* phys_alloc_page() {
    bool status = irq_save();
    spinlock_lock(&m_phys_lock);
    void* ptr = list_pop(&m_page_freelist);
    spinlock_unlock(&m_phys_lock);
    irq_restore(status);
    return ptr;
}

void phys_free_page(void* ptr) {
    bool status = irq_save();
    spinlock_lock(&m_phys_lock);
    list_add(&m_page_freelist, ptr);
    spinlock_unlock(&m_phys_lock);
    irq_restore(status);
}
