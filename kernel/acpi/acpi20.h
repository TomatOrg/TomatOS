#pragma once

#include "acpi.h"
#include <util/defs.h>

typedef struct acpi_2_0_generic_address_structure {
    uint8_t address_space_id;
    uint8_t register_bit_width;
    uint8_t register_bit_offset;
    uint8_t reserved;
    uint64_t address;
} PACKED acpi_2_0_generic_address_structure_t;

#define ACPI_2_0_SYSTEM_MEMORY              0
#define ACPI_2_0_SYSTEM_IO                  1
#define ACPI_2_0_PCI_CONFIGURATION_SPACE    2
#define ACPI_2_0_EMBEDDED_CONTROLLER        3
#define ACPI_2_0_SMBUS                      4
#define ACPI_2_0_FUNCTIONAL_FIXED_HARDWARE  0x7F


#define ACPI_2_0_FADT_REVISION  0x03

typedef struct acpi_2_0_fadt {
    acpi_descriptor_header_t header;
    uint32_t firmware_ctrl;
    uint32_t dsdt;
    uint8_t reserved0;
    uint8_t preferred_pm_profile;
    uint16_t sci_int;
    uint32_t smi_cmd;
    uint8_t acpi_enable;
    uint8_t acpi_disable;
    uint8_t s4_bios_req;
    uint8_t pstate_cnt;
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
    uint8_t cst_cnt;
    uint16_t p_lvl2_lat;
    uint16_t p_lvl3_lat;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t duty_offset;
    uint8_t duty_width;
    uint8_t day_alrm;
    uint8_t mon_alrm;
    uint8_t century;
    uint16_t ia_pc_boot_arch;
    uint8_t reserved1;
    uint32_t flags;
    acpi_2_0_generic_address_structure_t reset_reg;
    uint8_t reset_value;
    uint8_t reserved2[3];
    uint64_t x_firmware_ctrl;
    uint64_t x_dsdt;
    acpi_2_0_generic_address_structure_t x_pm1a_evt_blk;
    acpi_2_0_generic_address_structure_t x_pm1b_evt_blk;
    acpi_2_0_generic_address_structure_t x_pm1a_cnt_blk;
    acpi_2_0_generic_address_structure_t x_pm1b_cnt_blk;
    acpi_2_0_generic_address_structure_t x_pm2_cnt_blk;
    acpi_2_0_generic_address_structure_t x_pm_tmr_blk;
    acpi_2_0_generic_address_structure_t x_gpe0_blk;
    acpi_2_0_generic_address_structure_t x_gpe1_blk;
} PACKED acpi_2_0_fadt_t;

#define ACPI_2_0_WBINVD         BIT0
#define ACPI_2_0_WBINVD_FLUSH   BIT1
#define ACPI_2_0_PROC_C1        BIT2
#define ACPI_2_0_P_LVL2_UP      BIT3
#define ACPI_2_0_PWR_BUTTON     BIT4
#define ACPI_2_0_SLP_BUTTON     BIT5
#define ACPI_2_0_FIX_RTC        BIT6
#define ACPI_2_0_RTC_S4         BIT7
#define ACPI_2_0_TMR_VAL_EXT    BIT8
#define ACPI_2_0_DCK_CAP        BIT9
#define ACPI_2_0_RESET_REG_SUP  BIT10
#define ACPI_2_0_SEALED_CASE    BIT11
#define ACPI_2_0_HEADLESS       BIT12
#define ACPI_2_0_CPU_SW_SLP     BIT13