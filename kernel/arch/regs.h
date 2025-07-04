#pragma once

#include <stdint.h>
#include "lib/defs.h"

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

#define CR0_PG          BIT31
#define CR0_CD          BIT30
#define CR0_NW          BIT29
#define CR0_AM          BIT18
#define CR0_WP          BIT16
#define CR0_NE          BIT5
#define CR0_ET          BIT4
#define CR0_TS          BIT3
#define CR0_EM          BIT2
#define CR0_MP          BIT1
#define CR0_PE          BIT0

#define CR4_VME         BIT0
#define CR4_PVI         BIT1
#define CR4_TSD         BIT2
#define CR4_DE          BIT3
#define CR4_PSE         BIT4
#define CR4_PAE         BIT5
#define CR4_MCE         BIT6
#define CR4_PGE         BIT7
#define CR4_PCE         BIT8
#define CR4_OSFXSR      BIT9
#define CR4_OSXMMEXCPT  BIT10
#define CR4_UMIP        BIT11
#define CR4_LA57        BIT12
#define CR4_VMXE        BIT13
#define CR4_SMXE        BIT14
#define CR4_FSGSBASE    BIT16
#define CR4_PCIDE       BIT17
#define CR4_OSXSAVE     BIT18
#define CR4_KL          BIT19
#define CR4_SMEP        BIT20
#define CR4_SMAP        BIT21
#define CR4_PKE         BIT22
#define CR4_CET         BIT23
#define CR4_PKS         BIT24
#define CR4_UINTR       BIT25

typedef struct xsave_legacy_region {
    uint16_t fcw;
    uint16_t fsw;
    uint8_t ftw;
    uint8_t _reserved0;
    uint16_t fop;
    uint32_t fip;
    uint16_t fcs;
    uint16_t _reserved1;
    uint32_t fdp;
    uint16_t fds;
    uint16_t _reserved2;
    uint32_t mxscr;
    uint32_t mxcsr_mask;
    uint8_t st0mm0[10];
    uint8_t _reserved3[6];
    uint8_t st1mm1[10];
    uint8_t _reserved4[6];
    uint8_t st2mm2[10];
    uint8_t _reserved5[6];
    uint8_t st3mm3[10];
    uint8_t _reserved6[6];
    uint8_t st4mm4[10];
    uint8_t _reserved7[6];
    uint8_t st5mm5[10];
    uint8_t _reserved8[6];
    uint8_t st6mm6[10];
    uint8_t _reserved9[6];
    uint8_t st7mm7[10];
    uint8_t _reserved10[6];
    uint8_t xmm0[16];
    uint8_t xmm1[16];
    uint8_t xmm2[16];
    uint8_t xmm3[16];
    uint8_t xmm4[16];
    uint8_t xmm5[16];
    uint8_t xmm6[16];
    uint8_t xmm7[16];
    uint8_t xmm8[16];
    uint8_t xmm9[16];
    uint8_t xmm10[16];
    uint8_t xmm11[16];
    uint8_t xmm12[16];
    uint8_t xmm13[16];
    uint8_t xmm14[16];
    uint8_t xmm15[16];
    uint8_t _reserved11[96];
} xsave_legacy_region_t;
STATIC_ASSERT(sizeof(xsave_legacy_region_t) == 512);
