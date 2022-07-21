#include "acpi.h"

#include "kernel.h"

#include "acpi10.h"
#include "mem/mem.h"

struct acpi_descriptor_header* m_rsdt = NULL;

err_t init_acpi() {
    err_t err = NO_ERROR;

    // validate the structure
    // TODO: checksum
    acpi_1_0_rsdp_t* rsdp = (acpi_1_0_rsdp_t*)g_limine_rsdp.response->address;
    CHECK(rsdp->signature == ACPI_1_0_RSDP_SIGNATURE);

    // get and validate the rsdt
    m_rsdt = PHYS_TO_DIRECT(rsdp->rsdt_address);
    CHECK(m_rsdt->signature == ACPI_1_0_RSDT_SIGNATURE);
    CHECK(m_rsdt->revision >= ACPI_1_0_RSDT_REVISION);

cleanup:
    return err;
}

void* acpi_get_table(uint32_t signature) {
    size_t entry_count = (m_rsdt->length - sizeof(acpi_descriptor_header_t)) / sizeof(uint32_t);
    uint32_t* entries = (uint32_t*)(m_rsdt + 1);
    for (int i = 0; i < entry_count; i++) {
        acpi_descriptor_header_t* table = PHYS_TO_DIRECT(entries[i]);
        if (table->signature == signature) {
            return table;
        }
    }
    return NULL;
}
