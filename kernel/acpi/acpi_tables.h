#pragma once

#include "lib/defs.h"

#include <stdint.h>

#define ACPI_RSDP_SIGNATURE SIGNATURE_64('R', 'S', 'D', ' ', 'P', 'T', 'R', ' ')

typedef struct acpi_rsdp {
    uint64_t signature;
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;

    // from revision = 2
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t _reserved[3];
} PACKED acpi_rsdp_t;

typedef struct acpi_description_header {
    uint32_t signature;
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    uint8_t oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    char creator_id[4];
    uint32_t creator_revision;
} PACKED acpi_description_header_t;

#define ACPI_FACP_SIGNATURE SIGNATURE_32('F', 'A', 'C', 'P')

typedef struct acpi_facp {
    acpi_description_header_t header;
    uint32_t firmware_ctrl;
    uint32_t dsdt;
    uint8_t int_model;
    uint8_t _reserved1;
    uint16_t sci_int;
    uint32_t smi_cmd;
    uint8_t acpi_enable;
    uint8_t acpi_disable;
    uint8_t s4_bios_req;
    uint8_t _reserved2;
    uint32_t pm1a_evt_blk;
    uint32_t pm1b_evt_blk;
    uint32_t pm1a_cnt_blk;
    uint32_t pm1b_cnt_blk;
    uint32_t pm2_cnt_blk;
    uint32_t pm_tmr_blk;
    uint32_t gpe0_blk;
    uint32_t gpe1_blk;
    uint8_t pm1_evt_len;
    uint8_t pm1_cnt_len;
    uint8_t pm2_cnt_len;
    uint8_t pm_tmr_len;
    uint8_t gpe0_blk_len;
    uint8_t gpe1_blk_len;
    uint8_t gpe1_base;
    uint8_t _reserved3;
    uint16_t p_lvl2_lat;
    uint16_t o_lvl3_lat;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t duty_offset;
    uint8_t duty_width;
    uint8_t day_alrm;
    uint8_t mon_alrm;
    uint8_t century;
    uint8_t _reserved4;
    uint8_t _reserved5;
    uint8_t _reserved6;
    uint32_t flags;
} PACKED acpi_facp_t;
