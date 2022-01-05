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
