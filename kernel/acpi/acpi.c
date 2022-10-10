#include "acpi.h"

#include "kernel.h"

#include "acpi10.h"
#include "mem/mem.h"
#include "util/defs.h"

struct acpi_descriptor_header* m_rsdt = NULL;

err_t init_acpi() {
    err_t err = NO_ERROR;

    // validate the structure
    // TODO: checksum
    acpi_1_0_rsdp_t* rsdp = (acpi_1_0_rsdp_t*)g_limine_rsdp.response->address;
    // remember to map it, VirtualBox doesn't put it in any memory map entry
    // as the address isn't aligned, it may occupy two pages
    void* rsdp_page_vaddr = ALIGN_DOWN(rsdp, 4096); 
    CHECK_AND_RETHROW(vmm_map(DIRECT_TO_PHYS(rsdp_page_vaddr), rsdp_page_vaddr, 2, 0));
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
