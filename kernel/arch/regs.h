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
        uint32_t :10;            ///< Reserved.
        uint32_t WP:1;           ///< Write Protect.
        uint32_t :1;             ///< Reserved.
        uint32_t AM:1;           ///< Alignment Mask.
        uint32_t :10;            ///< Reserved.
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
        uint32_t :1;             ///< Reserved.
        uint32_t FSGSBASE:1;     ///< FSGSBASE Enable.
        uint32_t PCIDE:1;        ///< PCID Enable.
        uint32_t OSXSAVE:1;      ///< XSAVE and Processor Extended States Enable.
        uint32_t :1;             ///< Reserved.
        uint32_t SMEP:1;         ///< SMEP Enable.
        uint32_t SMAP:1;         ///< SMAP Enable.
        uint32_t PKE:1;          ///< Protection-Key Enable.
        uint32_t CET:1;          ///< Control-flow Enforcement Technology
        uint32_t PKS:1;          ///< Enable Protection Keys for Supervisor-Mode Pages
        uint32_t :7;             ///< Reserved.
    };
    uint32_t packed;
} PACKED cr4_t;
STATIC_ASSERT(sizeof(cr4_t) == sizeof(uint32_t));

typedef union rflags {
    struct {
        uint64_t CF         : 1;  ///< Carry Flag.
        uint64_t always_one : 1;  ///< Reserved.
        uint64_t PF         : 1;  ///< Parity Flag.
        uint64_t            : 1;  ///< Reserved.
        uint64_t AF         : 1;  ///< Auxiliary Carry Flag.
        uint64_t            : 1;  ///< Reserved.
        uint64_t ZF         : 1;  ///< Zero Flag.
        uint64_t SF         : 1;  ///< Sign Flag.
        uint64_t TF         : 1;  ///< Trap Flag.
        uint64_t IF         : 1;  ///< Interrupt Enable Flag.
        uint64_t DF         : 1;  ///< Direction Flag.
        uint64_t OF         : 1;  ///< Overflow Flag.
        uint64_t IOPL       : 2;  ///< I/O Privilege Level.
        uint64_t NT         : 1;  ///< Nested Task.
        uint64_t            : 1;  ///< Reserved.
        uint64_t RF         : 1;  ///< Resume Flag.
        uint64_t VM         : 1;  ///< Virtual 8086 Mode.
        uint64_t AC         : 1;  ///< Alignment Check.
        uint64_t VIF        : 1;  ///< Virtual Interrupt Flag.
        uint64_t VIP        : 1;  ///< Virtual Interrupt Pending.
        uint64_t ID         : 1;  ///< ID Flag.
        uint64_t            : 10; ///< Reserved.
    };
    uint64_t packed;
} PACKED rflags_t;
STATIC_ASSERT(sizeof(rflags_t) == sizeof(uint64_t));
