#pragma once

#include <util/defs.h>

#include "intrin.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define CPUID_EXTENDED_CPU_SIG 0x80000001

typedef union cpuid_extended_cpu_sig_edx {
    struct {
        uint32_t _reserved : 11;
        uint32_t syscall_sysret : 1;
        uint32_t _reserved2 : 8;
        uint32_t nx : 1;
        uint32_t _reserved3 : 5;
        uint32_t page_1gb : 1;
        uint32_t rdtscp : 1;
        uint32_t _reserved4 : 1;
        uint32_t lm : 1;
        uint32_t _reserved5 : 2;
    };
    uint32_t packed;
} PACKED cpuid_extended_cpu_sig_edx_t;
STATIC_ASSERT(sizeof(cpuid_extended_cpu_sig_edx_t) == sizeof(uint32_t));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define CPUID_EXTENDED_TIME_STAMP_COUNTER  0x80000007

typedef union cpuid_extended_time_stamp_counter_edx {
    struct {
        uint32_t _reserved : 8;
        uint32_t invariant_tsc : 1;
        uint32_t _reserved1 : 23;
    };
    uint32_t packed;
} PACKED cpuid_extended_time_stamp_counter_edx_t;
STATIC_ASSERT(sizeof(cpuid_extended_time_stamp_counter_edx_t) == sizeof(uint32_t));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define CPUID_VIR_PHY_ADDRESS_SIZE 0x80000008

typedef union cpuid_vir_phy_address_size_eax {
    struct {
        uint32_t physical_address_bits : 8;
        uint32_t linear_address_bits : 8;
        uint32_t _reserved : 16;
    };
    uint32_t packed;
} PACKED cpuid_vir_phy_address_size_eax_t;
STATIC_ASSERT(sizeof(cpuid_vir_phy_address_size_eax_t) == sizeof(uint32_t));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define CPUID_VERSION_INFO  0x01

typedef union cpuid_version_info_ebx {
    struct {
        uint32_t brand_index : 8;
        uint32_t cache_line_size : 8;
        uint32_t maximum_addressable_ids_for_logical_processors : 8;
        uint32_t initial_local_apic_id : 8;
    };
    uint32_t packed;
} PACKED cpuid_version_info_ebx_t;
STATIC_ASSERT(sizeof(cpuid_version_info_ebx_t) == sizeof(uint32_t));

typedef union cpuid_version_info_ecx {
    struct {
        uint32_t SSE3 : 1;
        uint32_t PCLMULQDQ : 1;
        uint32_t DTES64 : 1;
        uint32_t MONITOR : 1;
        uint32_t DS_CPL : 1;
        uint32_t VMX : 1;
        uint32_t SMX : 1;
        uint32_t EIST : 1;
        uint32_t TM2 : 1;
        uint32_t SSSE3 : 1;
        uint32_t CNXT_ID : 1;
        uint32_t SDBG : 1;
        uint32_t FMA : 1;
        uint32_t CMPXCHG16B : 1;
        uint32_t xTPR_Update_Control : 1;
        uint32_t PDCM : 1;
        uint32_t _reserved : 1;
        uint32_t PCID : 1;
        uint32_t DCA : 1;
        uint32_t SSE4_1 : 1;
        uint32_t SSE4_2 : 1;
        uint32_t x2APIC : 1;
        uint32_t MOVBE : 1;
        uint32_t POPCNT : 1;
        uint32_t TSC_Deadline : 1;
        uint32_t AESNI : 1;
        uint32_t XSAVE : 1;
        uint32_t OSXSAVE : 1;
        uint32_t AVX : 1;
        uint32_t F16C : 1;
        uint32_t RDRAND : 1;
        uint32_t _reserved1 : 1;
    };
    uint32_t packed;
} PACKED cpuid_version_info_ecx_t;
STATIC_ASSERT(sizeof(cpuid_version_info_ecx_t) == sizeof(uint32_t));

typedef union cpuid_version_info_edx {
    struct {
        uint32_t FPU : 1;
        uint32_t VME : 1;
        uint32_t DE : 1;
        uint32_t PSE : 1;
        uint32_t TSC : 1;
        uint32_t MSR : 1;
        uint32_t PAE : 1;
        uint32_t MCE : 1;
        uint32_t CX8 : 1;
        uint32_t APIC : 1;
        uint32_t _reserved1 : 1;
        uint32_t SEP : 1;
        uint32_t MTRR : 1;
        uint32_t PGE : 1;
        uint32_t MCA : 1;
        uint32_t CMOV : 1;
        uint32_t PAT : 1;
        uint32_t PSE_36 : 1;
        uint32_t PSN : 1;
        uint32_t CLFSH : 1;
        uint32_t _reserved2 : 1;
        uint32_t DS : 1;
        uint32_t ACPI : 1;
        uint32_t MMX : 1;
        uint32_t FXSR : 1;
        uint32_t SSE : 1;
        uint32_t SSE2 : 1;
        uint32_t SS : 1;
        uint32_t HTT : 1;
        uint32_t TM : 1;
        uint32_t _reserved3 : 1;
        uint32_t PBE : 1;
    };
    uint32_t packed;
} PACKED cpuid_version_info_edx_t;
STATIC_ASSERT(sizeof(cpuid_version_info_edx_t) == sizeof(uint32_t));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "intrin.h"

static inline void cpuid(uint32_t info_type, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) {
    int regs[4];
    __cpuid(regs, info_type);
    if (eax) *eax = regs[0];
    if (ebx) *ebx = regs[1];
    if (ecx) *ecx = regs[2];
    if (edx) *edx = regs[3];
}
