#pragma once

#include <util/defs.h>
#include <stdint.h>

typedef union cr0 {
    struct {
        uint32_t PE:1;           ///< Protection Enable.
        uint32_t MP:1;           ///< Monitor Coprocessor.
        uint32_t EM:1;           ///< Emulation.
        uint32_t TS:1;           ///< Task Switched.
        uint32_t ET:1;           ///< Extension Type.
        uint32_t NE:1;           ///< Numeric Error.
        uint32_t _reserved0:10;  ///< Reserved.
        uint32_t WP:1;           ///< Write Protect.
        uint32_t _reserved1:1;   ///< Reserved.
        uint32_t AM:1;           ///< Alignment Mask.
        uint32_t _reserved2:10;  ///< Reserved.
        uint32_t NW:1;           ///< Mot Write-through.
        uint32_t CD:1;           ///< Cache Disable.
        uint32_t PG:1;           ///< Paging.
    };
    uint32_t packed;
} PACKED cr0_t;
STATIC_ASSERT(sizeof(cr0_t) == sizeof(uint32_t));

typedef union cr4 {
    struct {
        uint32_t VME:1;          ///< Virtual-8086 Mode Extensions.
        uint32_t PVI:1;          ///< Protected-Mode Virtual Interrupts.
        uint32_t TSD:1;          ///< Time Stamp Disable.
        uint32_t DE:1;           ///< Debugging Extensions.
        uint32_t PSE:1;          ///< Page Size Extensions.
        uint32_t PAE:1;          ///< Physical Address Extension.
        uint32_t MCE:1;          ///< Machine Check Enable.
        uint32_t PGE:1;          ///< Page Global Enable.
        uint32_t PCE:1;          ///< Performance Monitoring Counter
                                 ///< Enable.
        uint32_t OSFXSR:1;       ///< Operating System Support for
                                 ///< FXSAVE and FXRSTOR instructions
        uint32_t OSXMMEXCPT:1;   ///< Operating System Support for
                                 ///< Unmasked SIMD Floating Point
                                 ///< Exceptions.
        uint32_t UMIP:1;         ///< User-Mode Instruction Prevention.
        uint32_t LA57:1;         ///< Linear Address 57bit.
        uint32_t VMXE:1;         ///< VMX Enable.
        uint32_t SMXE:1;         ///< SMX Enable.
        uint32_t _reserved3:1;   ///< Reserved.
        uint32_t FSGSBASE:1;     ///< FSGSBASE Enable.
        uint32_t PCIDE:1;        ///< PCID Enable.
        uint32_t OSXSAVE:1;      ///< XSAVE and Processor Extended States Enable.
        uint32_t _reserved4:1;   ///< Reserved.
        uint32_t SMEP:1;         ///< SMEP Enable.
        uint32_t SMAP:1;         ///< SMAP Enable.
        uint32_t PKE:1;          ///< Protection-Key Enable.
        uint32_t CET:1;          ///< Control-flow Enforcement Technology
        uint32_t PKS:1;          ///< Enable Protection Keys for Supervisor-Mode Pages
        uint32_t _reserved5:7;   ///< Reserved.
    };
    uint32_t packed;
} PACKED cr4_t;
STATIC_ASSERT(sizeof(cr4_t) == sizeof(uint32_t));
