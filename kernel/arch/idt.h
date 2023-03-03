#pragma once

#include <util/defs.h>

#include <stdint.h>
#include "regs.h"

/**
 * Initialize the IDT (interrupt handling)
 */
void init_idt();

#define INTERRUPT __attribute__ ((target("general-regs-only")))

#define EXCEPTION_DIVIDE_ERROR     0
#define EXCEPTION_DEBUG            1
#define EXCEPTION_NMI              2
#define EXCEPTION_BREAKPOINT       3
#define EXCEPTION_OVERFLOW         4
#define EXCEPTION_BOUND            5
#define EXCEPTION_INVALID_OPCODE   6
#define EXCEPTION_DOUBLE_FAULT     8
#define EXCEPTION_INVALID_TSS      10
#define EXCEPTION_SEG_NOT_PRESENT  11
#define EXCEPTION_STACK_FAULT      12
#define EXCEPTION_GP_FAULT         13
#define EXCEPTION_PAGE_FAULT       14
#define EXCEPTION_FP_ERROR         16
#define EXCEPTION_ALIGNMENT_CHECK  17
#define EXCEPTION_MACHINE_CHECK    18
#define EXCEPTION_SIMD             19

typedef struct interrupt_context {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t int_num;
    uint64_t rip;
    uint64_t cs;
    rflags_t rflags;
    uint64_t rsp;
    uint64_t ss;
} interrupt_context_t;

typedef struct exception_context {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t int_num;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    rflags_t rflags;
    uint64_t rsp;
    uint64_t ss;
} exception_context_t;

typedef union page_fault_error {
    struct {
        uint32_t present : 1;
        uint32_t write : 1;
        uint32_t user : 1;
        uint32_t reserved_write : 1;
        uint32_t instruction_fetch : 1;
        uint32_t protection_key : 1;
        uint32_t shadow_stack : 1;
        uint32_t sgx : 1;
    };
    uint32_t packed;
} PACKED page_fault_error_t;

void restore_interrupt_context(interrupt_context_t* ctx);
