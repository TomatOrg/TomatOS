#pragma once

#include <util/defs.h>

typedef struct acpi_common_header {
    uint32_t signature;
    uint32_t length;
} PACKED acpi_common_header_t;

typedef struct acpi_descriptor_header {
    uint32_t signature;
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    uint8_t oem_id[6];
    uint64_t oem_table_id;
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} PACKED acpi_descriptor_header_t;

#define ACPI_1_0_RSDP_SIGNATURE  SIGNATURE_64('R', 'S', 'D', ' ', 'P', 'T', 'R', ' ')

typedef struct acpi_1_0_rsdp {
    uint64_t signature;
    uint8_t checksum;
    uint8_t oem_id[6];
    uint8_t reserved;
    uint32_t rsdt_address;
} PACKED acpi_1_0_rsdp_t;

#define ACPI_1_0_RSDT_SIGNATURE  SIGNATURE_32('R', 'S', 'D', 'T')
#define ACPI_1_0_RSDT_REVISION  0x01

#define EFI_ACPI_1_0_FADT_SIGNATURE  SIGNATURE_32('F', 'A', 'C', 'P')
#define EFI_ACPI_1_0_FADT_REVISION  0x01

typedef struct acpi_1_0_fadt {
    acpi_descriptor_header_t header;
    uint32_t firmware_ctrl;
    uint32_t dsdt;
    int8_t int_model;
    int8_t _reserved1;
    uint16_t sci_int;
    uint32_t smi_cmd;
    int8_t acpi_enable;
    int8_t acpi_disable;
    int8_t s4_bios_req;
    int8_t _reserved2;
    uint32_t pm1a_evt_blk;
    uint32_t pm1b_evt_blk;
    uint32_t pm1a_cnt_blk;
    uint32_t pm1b_cnt_blk;
    uint32_t pm2_cnt_blk;
    uint32_t pm_tmr_blk;
    uint32_t gpe0_blk;
    uint32_t gpe1_blk;
    int8_t pm1_evt_len;
    int8_t pm1_cnt_len;
    int8_t pm2_cnt_len;
    int8_t pm_tmr_len;
    int8_t gpe0_blk_len;
    int8_t gpe1_blk_len;
    int8_t gpe1_base;
    int8_t _reserved3;
    uint16_t p_lvl2_lat;
    uint16_t p_lvl3_lat;
    uint16_t flush_size;
    uint16_t flush_stride;
    int8_t duty_offset;
    int8_t duty_width;
    int8_t day_alrm;
    int8_t mon_alrm;
    int8_t century;
    int8_t _reserved4;
    int8_t _reserved5;
    int8_t _reserved6;
    uint32_t flags;
} PACKED acpi_1_0_fadt_t;

#define ACPI_1_0_WBINVD        BIT0
#define ACPI_1_0_WBINVD_FLUSH  BIT1
#define ACPI_1_0_PROC_C1       BIT2
#define ACPI_1_0_P_LVL2_UP     BIT3
#define ACPI_1_0_PWR_BUTTON    BIT4
#define ACPI_1_0_SLP_BUTTON    BIT5
#define ACPI_1_0_FIX_RTC       BIT6
#define ACPI_1_0_RTC_S4        BIT7
#define ACPI_1_0_TMR_VAL_EXT   BIT8
#define ACPI_1_0_DCK_CAP       BIT9
