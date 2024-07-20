#include "acpi.h"
#include "limine.h"
#include "lib/defs.h"

#include "acpi_tables.h"
#include "mem/memory.h"
#include "arch/intrin.h"

/**
 * The frequency of the acpi timer
 */
#define ACPI_TIMER_FREQUENCY  3579545

/**
 * ACPI request from the bootloader
 */
LIMINE_REQUEST struct limine_rsdp_request m_limine_request = { .id = LIMINE_RSDP_REQUEST };

/**
 * The RSDP, saved for after boot
 */
static acpi_rsdp_t* m_rsdp = NULL;

static size_t m_rsdp_size;

/**
 * The timer port
 */
static uint16_t m_acpi_timer_port;

static err_t validate_acpi_table(acpi_description_header_t* header) {
    err_t err = NO_ERROR;

    // validate the header length
    CHECK(header->length >= sizeof(acpi_description_header_t));

    // validate the checksums
    uint8_t checksum = 0;
    for (size_t i = 0; i < header->length; i++) {
        checksum += *((uint8_t*)header + i);
    }
    CHECK(checksum == 0);

cleanup:
    return err;
}

err_t init_acpi() {
    err_t err = NO_ERROR;

    CHECK(m_limine_request.response != NULL);
    m_rsdp = m_limine_request.response->address;

    // calculate the size nicely
    m_rsdp_size = m_rsdp->revision >= 2 ? m_rsdp->length : 20;

    // save up the rsdp for future use
    CHECK(m_rsdp->signature == ACPI_RSDP_SIGNATURE);
    LOG_DEBUG("acpi: RSDP 0x%016X %06X (V%0X %.6s)", DIRECT_TO_PHYS(m_rsdp), m_rsdp_size, m_rsdp->revision, m_rsdp->oem_id);
    CHECK(m_rsdp->rsdt_address != 0);

    // the tables we need for early init
    acpi_facp_t* facp = NULL;

    // get either the xsdt or rsdt based on the revision
    acpi_description_header_t* xsdt = NULL;
    acpi_description_header_t* rsdt = NULL;
    if (m_rsdp->revision >= 2) {
        xsdt = PHYS_TO_DIRECT(m_rsdp->xsdt_address);
        LOG_DEBUG("acpi: %.4s 0x%016X %06X (V%0X %.6s %.4s %08X %.4s %08X)",
                  &xsdt->signature, DIRECT_TO_PHYS(xsdt), xsdt->length, xsdt->revision,
                  xsdt->oem_id, &xsdt->oem_table_id, xsdt->oem_revision,
                  &xsdt->creator_id, xsdt->creator_revision);
        RETHROW(validate_acpi_table(xsdt));
    } else {
        rsdt = PHYS_TO_DIRECT(m_rsdp->rsdt_address);
        LOG_DEBUG("acpi: %.4s 0x%016X %06X (V%0X %.6s %.4s %08X %.4s %08X)",
                  &rsdt->signature, DIRECT_TO_PHYS(rsdt), rsdt->length, rsdt->revision,
                  rsdt->oem_id, &rsdt->oem_table_id, rsdt->oem_revision,
                  &rsdt->creator_id, rsdt->creator_revision);
        RETHROW(validate_acpi_table(rsdt));
    }

    // calculate the entry count
    size_t entry_count;
    if (xsdt != NULL) {
        entry_count = (xsdt->length - sizeof(acpi_description_header_t)) / sizeof(void*);
    } else {
        entry_count = (rsdt->length - sizeof(acpi_description_header_t)) / sizeof(uint32_t);
    }

    // pass over the table, validating the tables and finding the
    // tables we do need right now
    for (size_t i = 0; i < entry_count; i++) {
        acpi_description_header_t* table;
        if (xsdt != NULL) {
            table = PHYS_TO_DIRECT(((acpi_description_header_t**)(xsdt + 1))[i]);
        } else if (rsdt != NULL) {
            table = PHYS_TO_DIRECT(((uint32_t*)(rsdt + 1))[i]);
        } else {
            CHECK_FAIL();
        }

        // print and validate
        LOG_DEBUG("acpi: %.4s 0x%016X %06X (V%0X %.6s %.4s %08X %.4s %08X)",
                  &table->signature, DIRECT_TO_PHYS(table), table->length, table->revision,
                  table->oem_id, &table->oem_table_id, table->oem_revision,
                  &table->creator_id, table->creator_revision);
        RETHROW(validate_acpi_table(table));

        // do we need this
        switch (table->signature) {
            case ACPI_FACP_SIGNATURE: facp = (acpi_facp_t*)table; break;
            default: break;
        }
    }

    // validate we got everything
    CHECK(facp != NULL);

    CHECK(facp->pm_tmr_blk != 0);
    CHECK(facp->pm_tmr_len == 4);
    m_acpi_timer_port = facp->pm_tmr_blk;

cleanup:
    return err;
}

static ALWAYS_INLINE uint32_t acpi_get_timer_tick() {
    return __indword(m_acpi_timer_port);
}

void acpi_stall(uint64_t microseconds) {
    uint32_t delay = (microseconds * ACPI_TIMER_FREQUENCY) / 1000000u;
    uint32_t times = delay >> 22;
    delay &= BIT22 - 1;
    do {
        uint32_t ticks = acpi_get_timer_tick() + delay;
        delay = BIT22;
        while (((ticks - acpi_get_timer_tick()) & BIT23) == 0) {
            cpu_relax();
        }
    } while (times-- > 0);
}
