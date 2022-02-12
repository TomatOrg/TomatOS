#pragma once

#include <util/defs.h>

#include <stdint.h>

/**
 * Initialize the IDT (interrupt handling)
 */
void init_idt();

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
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} interrupt_context_t;

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
