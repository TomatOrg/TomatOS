#include "idt.h"

#include <util/trace.h>
#include <util/defs.h>

#include <stddef.h>
#include <stdint.h>
#include <util/except.h>
#include <mem/vmm.h>
#include <stdnoreturn.h>
#include "intrin.h"

#define IDT_TYPE_TASK           0x5
#define IDT_TYPE_INTERRUPT_16   0x6
#define IDT_TYPE_TRAP_16        0x7
#define IDT_TYPE_INTERRUPT_32   0xE
#define IDT_TYPE_TRAP_32        0xF

typedef struct idt_entry {
    uint64_t handler_low : 16;
    uint64_t selector : 16;
    uint64_t ist : 3;
    uint64_t _zero1 : 5;
    uint64_t gate_type : 4;
    uint64_t _zero2 : 1;
    uint64_t ring : 2;
    uint64_t present : 1;
    uint64_t handler_high : 48;
    uint64_t _zero3 : 32;
} PACKED idt_entry_t;

typedef struct idt {
    uint16_t limit;
    idt_entry_t* base;
} PACKED idt_t;

asm(
    "common_exception_stub:\n"
    ".cfi_startproc simple\n"
    ".cfi_signal_frame\n"
    ".cfi_def_cfa %rsp, 0\n"
    ".cfi_offset %rip, 16\n"
    ".cfi_offset %rsp, 40\n"
    "cld\n"
    "pushq %rax\n"
    ".cfi_adjust_cfa_offset 8\n"
    ".cfi_rel_offset %rax, 0\n"
    "pushq %rbx\n"
    ".cfi_adjust_cfa_offset 8\n"
    ".cfi_rel_offset %rbx, 0\n"
    "pushq %rcx\n"
    ".cfi_adjust_cfa_offset 8\n"
    ".cfi_rel_offset %rcx, 0\n"
    "pushq %rdx\n"
    ".cfi_adjust_cfa_offset 8\n"
    ".cfi_rel_offset %rdx, 0\n"
    "pushq %rsi\n"
    ".cfi_adjust_cfa_offset 8\n"
    ".cfi_rel_offset %rsi, 0\n"
    "pushq %rdi\n"
    ".cfi_adjust_cfa_offset 8\n"
    ".cfi_rel_offset %rdi, 0\n"
    "pushq %rbp\n"
    ".cfi_adjust_cfa_offset 8\n"
    ".cfi_rel_offset %rbp, 0\n"
    "pushq %r8\n"
    ".cfi_adjust_cfa_offset 8\n"
    ".cfi_rel_offset %r8, 0\n"
    "pushq %r9\n"
    ".cfi_adjust_cfa_offset 8\n"
    ".cfi_rel_offset %r9, 0\n"
    "pushq %r10\n"
    ".cfi_adjust_cfa_offset 8\n"
    ".cfi_rel_offset %r10, 0\n"
    "pushq %r11\n"
    ".cfi_adjust_cfa_offset 8\n"
    ".cfi_rel_offset %r11, 0\n"
    "pushq %r12\n"
    ".cfi_adjust_cfa_offset 8\n"
    ".cfi_rel_offset %r12, 0\n"
    "pushq %r13\n"
    ".cfi_adjust_cfa_offset 8\n"
    ".cfi_rel_offset %r13, 0\n"
    "pushq %r14\n"
    ".cfi_adjust_cfa_offset 8\n"
    ".cfi_rel_offset %r14, 0\n"
    "pushq %r15\n"
    ".cfi_adjust_cfa_offset 8\n"
    ".cfi_rel_offset %r15, 0\n"
    "movq %ds, %rax\n"
    "pushq %rax\n"
    ".cfi_adjust_cfa_offset 8\n"
    "movw $16, %ax\n"
    "movw %ax, %ds\n"
    "movw %ax, %es\n"
    "movw %ax, %ss\n"
    "movq %rsp, %rdi\n"
    "call common_exception_handler\n"
    "popq %rax\n"
    ".cfi_adjust_cfa_offset -8\n"
    "movw %ax, %ds\n"
    "movw %ax, %es\n"
    "popq %r15\n"
    ".cfi_adjust_cfa_offset -8\n"
    ".cfi_restore %r15\n"
    "popq %r14\n"
    ".cfi_adjust_cfa_offset -8\n"
    ".cfi_restore %r14\n"
    "popq %r13\n"
    ".cfi_adjust_cfa_offset -8\n"
    ".cfi_restore %r13\n"
    "popq %r12\n"
    ".cfi_adjust_cfa_offset -8\n"
    ".cfi_restore %r12\n"
    "popq %r11\n"
    ".cfi_adjust_cfa_offset -8\n"
    ".cfi_restore %r11\n"
    "popq %r10\n"
    ".cfi_adjust_cfa_offset -8\n"
    ".cfi_restore %r10\n"
    "popq %r9\n"
    ".cfi_adjust_cfa_offset -8\n"
    ".cfi_restore %r9\n"
    "popq %r8\n"
    ".cfi_adjust_cfa_offset -8\n"
    ".cfi_restore %r8\n"
    "popq %rbp\n"
    ".cfi_adjust_cfa_offset -8\n"
    ".cfi_restore %rbp\n"
    "popq %rdi\n"
    ".cfi_adjust_cfa_offset -8\n"
    ".cfi_restore %rdi\n"
    "popq %rsi\n"
    ".cfi_adjust_cfa_offset -8\n"
    ".cfi_restore %rsi\n"
    "popq %rdx\n"
    ".cfi_adjust_cfa_offset -8\n"
    ".cfi_restore %rdx\n"
    "popq %rcx\n"
    ".cfi_adjust_cfa_offset -8\n"
    ".cfi_restore %rcx\n"
    "popq %rbx\n"
    ".cfi_adjust_cfa_offset -8\n"
    ".cfi_restore %rbx\n"
    "popq %rax\n"
    ".cfi_adjust_cfa_offset -8\n"
    ".cfi_restore %rax\n"
    "addq $16, %rsp\n"
    ".cfi_adjust_cfa_offset -16\n"
    "iretq\n"
    ".cfi_endproc\n"
);

#define EXCEPTION_STUB(num) \
    __attribute__((naked)) \
    static void interrupt_handle_##num() { \
        asm( \
            "pushq $0\n" \
            "pushq $" #num "\n" \
            "jmp common_exception_stub"); \
    }

#define EXCEPTION_ERROR_STUB(num) \
    __attribute__((naked)) \
    static void interrupt_handle_##num() { \
        asm( \
            "pushq $" #num "\n" \
            "jmp common_exception_stub"); \
    }

EXCEPTION_STUB(0x00);
EXCEPTION_STUB(0x01);
EXCEPTION_STUB(0x02);
EXCEPTION_STUB(0x03);
EXCEPTION_STUB(0x04);
EXCEPTION_STUB(0x05);
EXCEPTION_STUB(0x06);
EXCEPTION_STUB(0x07);
EXCEPTION_ERROR_STUB(0x08);
EXCEPTION_STUB(0x09);
EXCEPTION_ERROR_STUB(0x0a);
EXCEPTION_ERROR_STUB(0x0b);
EXCEPTION_ERROR_STUB(0x0c);
EXCEPTION_ERROR_STUB(0x0d);
EXCEPTION_ERROR_STUB(0x0e);
EXCEPTION_STUB(0x0f);
EXCEPTION_STUB(0x10);
EXCEPTION_ERROR_STUB(0x11);
EXCEPTION_STUB(0x12);
EXCEPTION_STUB(0x13);
EXCEPTION_STUB(0x14);
EXCEPTION_STUB(0x15);
EXCEPTION_STUB(0x16);
EXCEPTION_STUB(0x17);
EXCEPTION_STUB(0x18);
EXCEPTION_STUB(0x19);
EXCEPTION_STUB(0x1a);
EXCEPTION_STUB(0x1b);
EXCEPTION_STUB(0x1c);
EXCEPTION_STUB(0x1d);
EXCEPTION_ERROR_STUB(0x1e);
EXCEPTION_STUB(0x1f);

/**
 * All interrupt handler entries
 */
static idt_entry_t m_idt_entries[32];

/**
 * The idt
 */
static idt_t m_idt = {
    .limit = sizeof(m_idt_entries) - 1,
    .base = m_idt_entries
};

static void set_idt_entry(uint8_t i, void(*handler)()) {
    m_idt_entries[i].handler_low = (uint16_t) ((uintptr_t)handler & 0xFFFF);
    m_idt_entries[i].handler_high = (uint64_t) ((uintptr_t)handler >> 16);
    m_idt_entries[i].gate_type = IDT_TYPE_INTERRUPT_32;
    m_idt_entries[i].selector = 8;
    m_idt_entries[i].present = 1;
    m_idt_entries[i].ring = 0;
    m_idt_entries[i].ist = 0;
}

void init_idt() {
    set_idt_entry(0x0, interrupt_handle_0x00);
    set_idt_entry(0x1, interrupt_handle_0x01);
    set_idt_entry(0x2, interrupt_handle_0x02);
    set_idt_entry(0x3, interrupt_handle_0x03);
    set_idt_entry(0x4, interrupt_handle_0x04);
    set_idt_entry(0x5, interrupt_handle_0x05);
    set_idt_entry(0x6, interrupt_handle_0x06);
    set_idt_entry(0x7, interrupt_handle_0x07);
    set_idt_entry(0x8, interrupt_handle_0x08);
    set_idt_entry(0x9, interrupt_handle_0x09);
    set_idt_entry(0xa, interrupt_handle_0x0a);
    set_idt_entry(0xb, interrupt_handle_0x0b);
    set_idt_entry(0xc, interrupt_handle_0x0c);
    set_idt_entry(0xd, interrupt_handle_0x0d);
    set_idt_entry(0xe, interrupt_handle_0x0e);
    set_idt_entry(0xf, interrupt_handle_0x0f);
    set_idt_entry(0x10, interrupt_handle_0x10);
    set_idt_entry(0x11, interrupt_handle_0x11);
    set_idt_entry(0x12, interrupt_handle_0x12);
    set_idt_entry(0x13, interrupt_handle_0x13);
    set_idt_entry(0x14, interrupt_handle_0x14);
    set_idt_entry(0x15, interrupt_handle_0x15);
    set_idt_entry(0x16, interrupt_handle_0x16);
    set_idt_entry(0x17, interrupt_handle_0x17);
    set_idt_entry(0x18, interrupt_handle_0x18);
    set_idt_entry(0x19, interrupt_handle_0x19);
    set_idt_entry(0x1a, interrupt_handle_0x1a);
    set_idt_entry(0x1b, interrupt_handle_0x1b);
    set_idt_entry(0x1c, interrupt_handle_0x1c);
    set_idt_entry(0x1d, interrupt_handle_0x1d);
    set_idt_entry(0x1e, interrupt_handle_0x1e);
    set_idt_entry(0x1f, interrupt_handle_0x1f);
    asm volatile ("lidt %0" : : "m" (m_idt));
}

static const char* g_exception_name[] = {
    "#DE - Divide Error",
    "#DB - Debug",
    "Non Maskable Interrupt",
    "#BP - Breakpoint",
    "#OF - Overflow",
    "#BR - BOUND Range Exceeded",
    "#UD - Invalid Opcode",
    "#NM - Device Not Available",
    "#DF - Double Fault",
    "Coprocessor Segment Overrun",
    "#TS - Invalid TSS",
    "#NP - Segment Not Present",
    "#SS - Stack Fault Fault",
    "#GP - General Protection Fault",
    "#PF - Page-Fault",
    "Reserved",
    "#MF - x87 FPU Floating-Point Error",
    "#AC - Alignment Check",
    "#MC - Machine-Check",
    "#XM - SIMD Floating-Point Exception",
    "#VE - Virtualization Exception",
    "#CP - Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "#SX - Security Exception",
    "Reserved"
};

typedef struct exception_context {
    uint64_t ds;
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
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} exception_context_t;

static const char* g_pf_reason[] = {
    "Supervisory process tried to read a non-present phys entry",
    "Supervisory process tried to read a phys and caused a protection fault",
    "Supervisory process tried to write to a non-present phys entry",
    "Supervisory process tried to write a phys and caused a protection fault",
    "User process tried to read a non-present phys entry",
    "User process tried to read a phys and caused a protection fault",
    "User process tried to write to a non-present phys entry",
    "User process tried to write a phys and caused a protection fault"
};

static noreturn void default_exception_handler(exception_context_t* ctx) {
    // reset the lock so we can print
    ERROR("");
    ERROR("****************************************************");
    ERROR("Exception occurred: %s (%d)", g_exception_name[ctx->int_num], ctx->error_code);
    ERROR("****************************************************");
    ERROR("");
    if (ctx->int_num == 0xE) {
        if (ctx->error_code & BIT3) {
            ERROR("one or more phys directory entries contain reserved bits which are set to 1");
        } else {
            ERROR("%s", g_pf_reason[ctx->error_code & 0b111]);
        }
        ERROR("");
    }

    // registers
    ERROR("RAX=%016p  RBX=%016p RCX=%016p RDX=%016p", ctx->rax, ctx->rbx, ctx->rcx, ctx->rdx);
    ERROR("RSI=%016p  RDI=%016p RBP=%016p RSP=%016p", ctx->rsi, ctx->rdi, ctx->rbp, ctx->rsp);
    ERROR("R8 =%016p  R9 =%016p R10=%016p R11=%016p", ctx->r8 , ctx->r9 , ctx->r10, ctx->r11);
    ERROR("R12=%016p  R13=%016p R14=%016p R15=%016p", ctx->r12, ctx->r13, ctx->r14, ctx->r15);
    ERROR("RIP=%016p RFL=%b", ctx->rip, ctx->rflags);
    ERROR("CR0=%08x CR2=%016p CR3=%016p CR4=%08x", __readcr0(), __readcr2(), __readcr3(), __readcr4());

    // stop
    ERROR("Halting :(");
    while(1) asm("hlt");
}

__attribute__((used))
void common_exception_handler(exception_context_t* ctx) {
    err_t err = NO_ERROR;

    if (ctx->int_num == 14) {
        CHECK(ctx->error_code <= 4, "Usermode error?");
        bool write = ctx->error_code == 2 || ctx->error_code == 3;
        bool present = ctx->error_code == 1 || ctx->error_code == 3;
        CHECK_AND_RETHROW(vmm_page_fault_handler(__readcr2(), write, present));
    } else {
        default_exception_handler(ctx);
    }

cleanup:
    if (IS_ERROR(err)) {
        default_exception_handler(ctx);
    }
}
