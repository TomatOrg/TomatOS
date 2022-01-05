#pragma once

#include <util/defs.h>

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

#include "intrin.h"

static inline void cpuid(uint32_t info_type, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) {
    int regs[4];
    __cpuid(regs, info_type);
    if (eax) *eax = regs[0];
    if (ebx) *ebx = regs[1];
    if (ecx) *ecx = regs[2];
    if (edx) *edx = regs[3];
}
