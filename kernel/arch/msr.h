#pragma once

#include <util/defs.h>

#define MSR_IA32_EFER  0xC0000080

typedef union msr_efer {
    struct {
        uint64_t SCE : 1;
        uint64_t _reserved1 : 7;
        uint64_t LME : 1;
        uint64_t _reserved2 : 1;
        uint64_t LMA : 1;
        uint64_t NXE : 1;
        uint64_t _reserved3 : 52;
    };
    uint64_t packed;
} PACKED msr_efer_t;
STATIC_ASSERT(sizeof(msr_efer_t) == sizeof(uint64_t));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define MSR_IA32_PAT  0x00000277
typedef enum msr_pat_type {
    PAT_TYPE_UNCACHEABLE = 0,
    PAT_TYPE_WRITE_COMBINING = 1,
    PAT_TYPE_WRITE_THROUGH = 4,
    PAT_TYPE_WRITE_PROTECTED = 5,
    PAT_TYPE_WRITE_BACK = 6,
    PAT_TYPE_UNCACHED = 7,
} msr_pat_type_t;

typedef union msr_pat {
    struct {
        uint64_t pa0 : 3;
        uint64_t _reserved0 : 5;
        uint64_t pa1 : 3;
        uint64_t _reserved1 : 5;
        uint64_t pa2 : 3;
        uint64_t _reserved2 : 5;
        uint64_t pa3 : 3;
        uint64_t _reserved3 : 5;
        uint64_t pa4 : 3;
        uint64_t _reserved4 : 5;
        uint64_t pa5 : 3;
        uint64_t _reserved5 : 5;
        uint64_t pa6 : 3;
        uint64_t _reserved6 : 5;
        uint64_t pa7 : 3;
        uint64_t _reserved7 : 5;
    };
    uint64_t packed;
} PACKED msr_pat_t;
STATIC_ASSERT(sizeof(msr_pat_t) == sizeof(uint64_t));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define MSR_IA32_APIC_BASE  0x0000001B

typedef union msr_apic_base {
    struct {
        uint64_t _reserved1 : 8;
        uint64_t bsp : 1;
        uint64_t _reserved2 : 1;
        uint64_t extd : 1;
        uint64_t en : 1;
        uint64_t apic_base : 52;
    };
    uint64_t packed;
} PACKED msr_apic_base_t;
STATIC_ASSERT(sizeof(msr_apic_base_t) == sizeof(uint64_t));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define MSR_IA32_FS_BASE  0xC0000100

#define MSR_IA32_GS_BASE  0xC0000101

#define MSR_IA32_TSC_AUX  0xC0000103

#define MSR_IA32_TSC_DEADLINE  0x000006E0
