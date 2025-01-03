#pragma once

#include "regs.h"
#include "lib/defs.h"

typedef struct interrupt_frame {
    uint64_t rip;
    uint64_t cs;
    rflags_t rflags;
    uint64_t rsp;
    uint64_t ss;
} interrupt_frame_t;

#define EXCEPT_IA32_DIVIDE_ERROR     0
#define EXCEPT_IA32_DEBUG            1
#define EXCEPT_IA32_NMI              2
#define EXCEPT_IA32_BREAKPOINT       3
#define EXCEPT_IA32_OVERFLOW         4
#define EXCEPT_IA32_BOUND            5
#define EXCEPT_IA32_INVALID_OPCODE   6
#define EXCEPT_IA32_DOUBLE_FAULT     8
#define EXCEPT_IA32_INVALID_TSS      10
#define EXCEPT_IA32_SEG_NOT_PRESENT  11
#define EXCEPT_IA32_STACK_FAULT      12
#define EXCEPT_IA32_GP_FAULT         13
#define EXCEPT_IA32_PAGE_FAULT       14
#define EXCEPT_IA32_FP_ERROR         16
#define EXCEPT_IA32_ALIGNMENT_CHECK  17
#define EXCEPT_IA32_MACHINE_CHECK    18
#define EXCEPT_IA32_SIMD             19

void init_idt();
