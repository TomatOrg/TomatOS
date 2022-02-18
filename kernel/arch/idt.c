#include "idt.h"

#include <util/trace.h>
#include <util/defs.h>

#include <stddef.h>
#include <stdint.h>
#include <util/except.h>
#include <mem/vmm.h>
#include <stdnoreturn.h>
#include "intrin.h"
#include "sync/spinlock.h"
#include "debug/debug.h"
#include "msr.h"
#include "threading/thread.h"
#include "apic.h"
#include "threading/scheduler.h"

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Exception handling code
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
    "movq %rsp, %rdi\n"
    "call common_exception_handler\n"
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

/**
 * Exception spinlock, so the exception output will be synced nicely even on multi
 * core crashes
 */
static spinlock_t m_exception_lock = INIT_SPINLOCK();

/**
 * The default exception handler, simply panics...
 */
static noreturn void default_exception_handler(exception_context_t* ctx) {
    spinlock_lock(&m_exception_lock);

    // reset the spinlock so we can print
    ERROR("");
    ERROR("****************************************************");
    ERROR("Exception occurred: %s (%d)", g_exception_name[ctx->int_num], ctx->error_code);
    ERROR("****************************************************");
    ERROR("");

    page_fault_error_t page_fault_code;
    if (ctx->int_num == 0xE) {
        page_fault_code = (page_fault_error_t) { .packed = ctx->error_code };
        if (page_fault_code.reserved_write) {
            ERROR("one or more page directory entries contain reserved bits which are set to 1");
        } else if (page_fault_code.instruction_fetch) {
            ERROR("tried to run non-executable code");
        } else {
            const char* rw = page_fault_code.write ? "write" : "read";
            if (page_fault_code.present) {
                ERROR("%s from non-present page", rw);
            } else {
                ERROR("page-protection violation when %s page", rw);
            }
        }
        ERROR("");
    }

    // check if we have threading_old already
//    if (__readmsr(MSR_IA32_GS_BASE) != 0) {
//        thread_t* thread = get_current_thread();
//        if (thread != NULL) {
//            ERROR("Thread: `%.*s`", sizeof(thread->name), thread->name);
//            ERROR("\tPriority: %d (actual %d)", thread->priority, __readcr8());
//            ERROR("");
//        }
//    }

    // registers
    ERROR("RAX=%016p RBX=%016p RCX=%016p RDX=%016p", ctx->rax, ctx->rbx, ctx->rcx, ctx->rdx);
    ERROR("RSI=%016p RDI=%016p RBP=%016p RSP=%016p", ctx->rsi, ctx->rdi, ctx->rbp, ctx->rsp);
    ERROR("R8 =%016p R9 =%016p R10=%016p R11=%016p", ctx->r8 , ctx->r9 , ctx->r10, ctx->r11);
    ERROR("R12=%016p R13=%016p R14=%016p R15=%016p", ctx->r12, ctx->r13, ctx->r14, ctx->r15);
    ERROR("RIP=%016p RFL=%b", ctx->rip, ctx->rflags);
    ERROR("FS =%016p GS =%016p", __readmsr(MSR_IA32_FS_BASE), __readmsr(MSR_IA32_GS_BASE));
    ERROR("CR0=%08x CR2=%016p CR3=%016p CR4=%08x", __readcr0(), __readcr2(), __readcr3(), __readcr4());

    // decode some opcodes for better debugging
    if (ctx->int_num != 0xE || !page_fault_code.instruction_fetch) {
        ERROR("");
        ERROR("Code:");
        debug_disasm_at((void*)ctx->rip, 5);
    }

    ERROR("");

    // stop
    ERROR("Halting :(");
    spinlock_unlock(&m_exception_lock);
    while(1) asm("hlt");
}

__attribute__((used))
void common_exception_handler(exception_context_t* ctx) {
    err_t err = NO_ERROR;

    if (ctx->int_num == 14) {
        page_fault_error_t error = { .packed = ctx->error_code };
        CHECK_AND_RETHROW(vmm_page_fault_handler(__readcr2(), error.write, error.present));
    } else {
        default_exception_handler(ctx);
    }

    cleanup:
    if (IS_ERROR(err)) {
        default_exception_handler(ctx);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Interrupt handling code
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

asm(
    "common_interrupt_stub:\n"
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
    "movq %rsp, %rdi\n"
    "call common_interrupt_handler\n"
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
    "addq $8, %rsp\n"
    ".cfi_adjust_cfa_offset -16\n"
    "iretq\n"
    ".cfi_endproc\n"
);

#define INTERRUPT_HANDLER(num) \
    __attribute__((naked)) \
    static void interrupt_handle_##num() { \
        asm( \
            "pushq $" #num "\n" \
            "jmp common_interrupt_stub"); \
    }

INTERRUPT_HANDLER(0x20)
INTERRUPT_HANDLER(0x21)
INTERRUPT_HANDLER(0x22)
INTERRUPT_HANDLER(0x23)
INTERRUPT_HANDLER(0x24)
INTERRUPT_HANDLER(0x25)
INTERRUPT_HANDLER(0x26)
INTERRUPT_HANDLER(0x27)
INTERRUPT_HANDLER(0x28)
INTERRUPT_HANDLER(0x29)
INTERRUPT_HANDLER(0x2a)
INTERRUPT_HANDLER(0x2b)
INTERRUPT_HANDLER(0x2c)
INTERRUPT_HANDLER(0x2d)
INTERRUPT_HANDLER(0x2e)
INTERRUPT_HANDLER(0x2f)
INTERRUPT_HANDLER(0x30)
INTERRUPT_HANDLER(0x31)
INTERRUPT_HANDLER(0x32)
INTERRUPT_HANDLER(0x33)
INTERRUPT_HANDLER(0x34)
INTERRUPT_HANDLER(0x35)
INTERRUPT_HANDLER(0x36)
INTERRUPT_HANDLER(0x37)
INTERRUPT_HANDLER(0x38)
INTERRUPT_HANDLER(0x39)
INTERRUPT_HANDLER(0x3a)
INTERRUPT_HANDLER(0x3b)
INTERRUPT_HANDLER(0x3c)
INTERRUPT_HANDLER(0x3d)
INTERRUPT_HANDLER(0x3e)
INTERRUPT_HANDLER(0x3f)
INTERRUPT_HANDLER(0x40)
INTERRUPT_HANDLER(0x41)
INTERRUPT_HANDLER(0x42)
INTERRUPT_HANDLER(0x43)
INTERRUPT_HANDLER(0x44)
INTERRUPT_HANDLER(0x45)
INTERRUPT_HANDLER(0x46)
INTERRUPT_HANDLER(0x47)
INTERRUPT_HANDLER(0x48)
INTERRUPT_HANDLER(0x49)
INTERRUPT_HANDLER(0x4a)
INTERRUPT_HANDLER(0x4b)
INTERRUPT_HANDLER(0x4c)
INTERRUPT_HANDLER(0x4d)
INTERRUPT_HANDLER(0x4e)
INTERRUPT_HANDLER(0x4f)
INTERRUPT_HANDLER(0x50)
INTERRUPT_HANDLER(0x51)
INTERRUPT_HANDLER(0x52)
INTERRUPT_HANDLER(0x53)
INTERRUPT_HANDLER(0x54)
INTERRUPT_HANDLER(0x55)
INTERRUPT_HANDLER(0x56)
INTERRUPT_HANDLER(0x57)
INTERRUPT_HANDLER(0x58)
INTERRUPT_HANDLER(0x59)
INTERRUPT_HANDLER(0x5a)
INTERRUPT_HANDLER(0x5b)
INTERRUPT_HANDLER(0x5c)
INTERRUPT_HANDLER(0x5d)
INTERRUPT_HANDLER(0x5e)
INTERRUPT_HANDLER(0x5f)
INTERRUPT_HANDLER(0x60)
INTERRUPT_HANDLER(0x61)
INTERRUPT_HANDLER(0x62)
INTERRUPT_HANDLER(0x63)
INTERRUPT_HANDLER(0x64)
INTERRUPT_HANDLER(0x65)
INTERRUPT_HANDLER(0x66)
INTERRUPT_HANDLER(0x67)
INTERRUPT_HANDLER(0x68)
INTERRUPT_HANDLER(0x69)
INTERRUPT_HANDLER(0x6a)
INTERRUPT_HANDLER(0x6b)
INTERRUPT_HANDLER(0x6c)
INTERRUPT_HANDLER(0x6d)
INTERRUPT_HANDLER(0x6e)
INTERRUPT_HANDLER(0x6f)
INTERRUPT_HANDLER(0x70)
INTERRUPT_HANDLER(0x71)
INTERRUPT_HANDLER(0x72)
INTERRUPT_HANDLER(0x73)
INTERRUPT_HANDLER(0x74)
INTERRUPT_HANDLER(0x75)
INTERRUPT_HANDLER(0x76)
INTERRUPT_HANDLER(0x77)
INTERRUPT_HANDLER(0x78)
INTERRUPT_HANDLER(0x79)
INTERRUPT_HANDLER(0x7a)
INTERRUPT_HANDLER(0x7b)
INTERRUPT_HANDLER(0x7c)
INTERRUPT_HANDLER(0x7d)
INTERRUPT_HANDLER(0x7e)
INTERRUPT_HANDLER(0x7f)
INTERRUPT_HANDLER(0x80)
INTERRUPT_HANDLER(0x81)
INTERRUPT_HANDLER(0x82)
INTERRUPT_HANDLER(0x83)
INTERRUPT_HANDLER(0x84)
INTERRUPT_HANDLER(0x85)
INTERRUPT_HANDLER(0x86)
INTERRUPT_HANDLER(0x87)
INTERRUPT_HANDLER(0x88)
INTERRUPT_HANDLER(0x89)
INTERRUPT_HANDLER(0x8a)
INTERRUPT_HANDLER(0x8b)
INTERRUPT_HANDLER(0x8c)
INTERRUPT_HANDLER(0x8d)
INTERRUPT_HANDLER(0x8e)
INTERRUPT_HANDLER(0x8f)
INTERRUPT_HANDLER(0x90)
INTERRUPT_HANDLER(0x91)
INTERRUPT_HANDLER(0x92)
INTERRUPT_HANDLER(0x93)
INTERRUPT_HANDLER(0x94)
INTERRUPT_HANDLER(0x95)
INTERRUPT_HANDLER(0x96)
INTERRUPT_HANDLER(0x97)
INTERRUPT_HANDLER(0x98)
INTERRUPT_HANDLER(0x99)
INTERRUPT_HANDLER(0x9a)
INTERRUPT_HANDLER(0x9b)
INTERRUPT_HANDLER(0x9c)
INTERRUPT_HANDLER(0x9d)
INTERRUPT_HANDLER(0x9e)
INTERRUPT_HANDLER(0x9f)
INTERRUPT_HANDLER(0xa0)
INTERRUPT_HANDLER(0xa1)
INTERRUPT_HANDLER(0xa2)
INTERRUPT_HANDLER(0xa3)
INTERRUPT_HANDLER(0xa4)
INTERRUPT_HANDLER(0xa5)
INTERRUPT_HANDLER(0xa6)
INTERRUPT_HANDLER(0xa7)
INTERRUPT_HANDLER(0xa8)
INTERRUPT_HANDLER(0xa9)
INTERRUPT_HANDLER(0xaa)
INTERRUPT_HANDLER(0xab)
INTERRUPT_HANDLER(0xac)
INTERRUPT_HANDLER(0xad)
INTERRUPT_HANDLER(0xae)
INTERRUPT_HANDLER(0xaf)
INTERRUPT_HANDLER(0xb0)
INTERRUPT_HANDLER(0xb1)
INTERRUPT_HANDLER(0xb2)
INTERRUPT_HANDLER(0xb3)
INTERRUPT_HANDLER(0xb4)
INTERRUPT_HANDLER(0xb5)
INTERRUPT_HANDLER(0xb6)
INTERRUPT_HANDLER(0xb7)
INTERRUPT_HANDLER(0xb8)
INTERRUPT_HANDLER(0xb9)
INTERRUPT_HANDLER(0xba)
INTERRUPT_HANDLER(0xbb)
INTERRUPT_HANDLER(0xbc)
INTERRUPT_HANDLER(0xbd)
INTERRUPT_HANDLER(0xbe)
INTERRUPT_HANDLER(0xbf)
INTERRUPT_HANDLER(0xc0)
INTERRUPT_HANDLER(0xc1)
INTERRUPT_HANDLER(0xc2)
INTERRUPT_HANDLER(0xc3)
INTERRUPT_HANDLER(0xc4)
INTERRUPT_HANDLER(0xc5)
INTERRUPT_HANDLER(0xc6)
INTERRUPT_HANDLER(0xc7)
INTERRUPT_HANDLER(0xc8)
INTERRUPT_HANDLER(0xc9)
INTERRUPT_HANDLER(0xca)
INTERRUPT_HANDLER(0xcb)
INTERRUPT_HANDLER(0xcc)
INTERRUPT_HANDLER(0xcd)
INTERRUPT_HANDLER(0xce)
INTERRUPT_HANDLER(0xcf)
INTERRUPT_HANDLER(0xd0)
INTERRUPT_HANDLER(0xd1)
INTERRUPT_HANDLER(0xd2)
INTERRUPT_HANDLER(0xd3)
INTERRUPT_HANDLER(0xd4)
INTERRUPT_HANDLER(0xd5)
INTERRUPT_HANDLER(0xd6)
INTERRUPT_HANDLER(0xd7)
INTERRUPT_HANDLER(0xd8)
INTERRUPT_HANDLER(0xd9)
INTERRUPT_HANDLER(0xda)
INTERRUPT_HANDLER(0xdb)
INTERRUPT_HANDLER(0xdc)
INTERRUPT_HANDLER(0xdd)
INTERRUPT_HANDLER(0xde)
INTERRUPT_HANDLER(0xdf)
INTERRUPT_HANDLER(0xe0)
INTERRUPT_HANDLER(0xe1)
INTERRUPT_HANDLER(0xe2)
INTERRUPT_HANDLER(0xe3)
INTERRUPT_HANDLER(0xe4)
INTERRUPT_HANDLER(0xe5)
INTERRUPT_HANDLER(0xe6)
INTERRUPT_HANDLER(0xe7)
INTERRUPT_HANDLER(0xe8)
INTERRUPT_HANDLER(0xe9)
INTERRUPT_HANDLER(0xea)
INTERRUPT_HANDLER(0xeb)
INTERRUPT_HANDLER(0xec)
INTERRUPT_HANDLER(0xed)
INTERRUPT_HANDLER(0xee)
INTERRUPT_HANDLER(0xef)
INTERRUPT_HANDLER(0xf0)
INTERRUPT_HANDLER(0xf1)
INTERRUPT_HANDLER(0xf2)
INTERRUPT_HANDLER(0xf3)
INTERRUPT_HANDLER(0xf4)
INTERRUPT_HANDLER(0xf5)
INTERRUPT_HANDLER(0xf6)
INTERRUPT_HANDLER(0xf7)
INTERRUPT_HANDLER(0xf8)
INTERRUPT_HANDLER(0xf9)
INTERRUPT_HANDLER(0xfa)
INTERRUPT_HANDLER(0xfb)
INTERRUPT_HANDLER(0xfc)
INTERRUPT_HANDLER(0xfd)
INTERRUPT_HANDLER(0xfe)
INTERRUPT_HANDLER(0xff)

__attribute__((used))
void common_interrupt_handler(interrupt_context_t* ctx) {
    err_t err = NO_ERROR;

    switch (ctx->int_num) {
        case IRQ_PREEMPT: {
            scheduler_on_schedule(ctx);
            lapic_eoi();
        } break;

        case IRQ_SCHEDULE: {
            scheduler_on_schedule(ctx);
        } break;

        case IRQ_YIELD: {
            scheduler_on_yield(ctx);
        } break;

        case IRQ_PARK: {
            scheduler_on_park(ctx);
        } break;

        case IRQ_DROP: {
            scheduler_on_drop(ctx);
        } break;

        default: {
            CHECK_FAIL("Unknown interrupt %d", ctx->int_num);
        } break;
    }

cleanup:
    if (IS_ERROR(err)) {
        while (1) asm("hlt");
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Generic IDT code
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/**
 * All interrupt handler entries
 */
static idt_entry_t m_idt_entries[256];

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
    set_idt_entry(0x20, interrupt_handle_0x20);
    set_idt_entry(0x21, interrupt_handle_0x21);
    set_idt_entry(0x22, interrupt_handle_0x22);
    set_idt_entry(0x23, interrupt_handle_0x23);
    set_idt_entry(0x24, interrupt_handle_0x24);
    set_idt_entry(0x25, interrupt_handle_0x25);
    set_idt_entry(0x26, interrupt_handle_0x26);
    set_idt_entry(0x27, interrupt_handle_0x27);
    set_idt_entry(0x28, interrupt_handle_0x28);
    set_idt_entry(0x29, interrupt_handle_0x29);
    set_idt_entry(0x2a, interrupt_handle_0x2a);
    set_idt_entry(0x2b, interrupt_handle_0x2b);
    set_idt_entry(0x2c, interrupt_handle_0x2c);
    set_idt_entry(0x2d, interrupt_handle_0x2d);
    set_idt_entry(0x2e, interrupt_handle_0x2e);
    set_idt_entry(0x2f, interrupt_handle_0x2f);
    set_idt_entry(0x30, interrupt_handle_0x30);
    set_idt_entry(0x31, interrupt_handle_0x31);
    set_idt_entry(0x32, interrupt_handle_0x32);
    set_idt_entry(0x33, interrupt_handle_0x33);
    set_idt_entry(0x34, interrupt_handle_0x34);
    set_idt_entry(0x35, interrupt_handle_0x35);
    set_idt_entry(0x36, interrupt_handle_0x36);
    set_idt_entry(0x37, interrupt_handle_0x37);
    set_idt_entry(0x38, interrupt_handle_0x38);
    set_idt_entry(0x39, interrupt_handle_0x39);
    set_idt_entry(0x3a, interrupt_handle_0x3a);
    set_idt_entry(0x3b, interrupt_handle_0x3b);
    set_idt_entry(0x3c, interrupt_handle_0x3c);
    set_idt_entry(0x3d, interrupt_handle_0x3d);
    set_idt_entry(0x3e, interrupt_handle_0x3e);
    set_idt_entry(0x3f, interrupt_handle_0x3f);
    set_idt_entry(0x40, interrupt_handle_0x40);
    set_idt_entry(0x41, interrupt_handle_0x41);
    set_idt_entry(0x42, interrupt_handle_0x42);
    set_idt_entry(0x43, interrupt_handle_0x43);
    set_idt_entry(0x44, interrupt_handle_0x44);
    set_idt_entry(0x45, interrupt_handle_0x45);
    set_idt_entry(0x46, interrupt_handle_0x46);
    set_idt_entry(0x47, interrupt_handle_0x47);
    set_idt_entry(0x48, interrupt_handle_0x48);
    set_idt_entry(0x49, interrupt_handle_0x49);
    set_idt_entry(0x4a, interrupt_handle_0x4a);
    set_idt_entry(0x4b, interrupt_handle_0x4b);
    set_idt_entry(0x4c, interrupt_handle_0x4c);
    set_idt_entry(0x4d, interrupt_handle_0x4d);
    set_idt_entry(0x4e, interrupt_handle_0x4e);
    set_idt_entry(0x4f, interrupt_handle_0x4f);
    set_idt_entry(0x50, interrupt_handle_0x50);
    set_idt_entry(0x51, interrupt_handle_0x51);
    set_idt_entry(0x52, interrupt_handle_0x52);
    set_idt_entry(0x53, interrupt_handle_0x53);
    set_idt_entry(0x54, interrupt_handle_0x54);
    set_idt_entry(0x55, interrupt_handle_0x55);
    set_idt_entry(0x56, interrupt_handle_0x56);
    set_idt_entry(0x57, interrupt_handle_0x57);
    set_idt_entry(0x58, interrupt_handle_0x58);
    set_idt_entry(0x59, interrupt_handle_0x59);
    set_idt_entry(0x5a, interrupt_handle_0x5a);
    set_idt_entry(0x5b, interrupt_handle_0x5b);
    set_idt_entry(0x5c, interrupt_handle_0x5c);
    set_idt_entry(0x5d, interrupt_handle_0x5d);
    set_idt_entry(0x5e, interrupt_handle_0x5e);
    set_idt_entry(0x5f, interrupt_handle_0x5f);
    set_idt_entry(0x60, interrupt_handle_0x60);
    set_idt_entry(0x61, interrupt_handle_0x61);
    set_idt_entry(0x62, interrupt_handle_0x62);
    set_idt_entry(0x63, interrupt_handle_0x63);
    set_idt_entry(0x64, interrupt_handle_0x64);
    set_idt_entry(0x65, interrupt_handle_0x65);
    set_idt_entry(0x66, interrupt_handle_0x66);
    set_idt_entry(0x67, interrupt_handle_0x67);
    set_idt_entry(0x68, interrupt_handle_0x68);
    set_idt_entry(0x69, interrupt_handle_0x69);
    set_idt_entry(0x6a, interrupt_handle_0x6a);
    set_idt_entry(0x6b, interrupt_handle_0x6b);
    set_idt_entry(0x6c, interrupt_handle_0x6c);
    set_idt_entry(0x6d, interrupt_handle_0x6d);
    set_idt_entry(0x6e, interrupt_handle_0x6e);
    set_idt_entry(0x6f, interrupt_handle_0x6f);
    set_idt_entry(0x70, interrupt_handle_0x70);
    set_idt_entry(0x71, interrupt_handle_0x71);
    set_idt_entry(0x72, interrupt_handle_0x72);
    set_idt_entry(0x73, interrupt_handle_0x73);
    set_idt_entry(0x74, interrupt_handle_0x74);
    set_idt_entry(0x75, interrupt_handle_0x75);
    set_idt_entry(0x76, interrupt_handle_0x76);
    set_idt_entry(0x77, interrupt_handle_0x77);
    set_idt_entry(0x78, interrupt_handle_0x78);
    set_idt_entry(0x79, interrupt_handle_0x79);
    set_idt_entry(0x7a, interrupt_handle_0x7a);
    set_idt_entry(0x7b, interrupt_handle_0x7b);
    set_idt_entry(0x7c, interrupt_handle_0x7c);
    set_idt_entry(0x7d, interrupt_handle_0x7d);
    set_idt_entry(0x7e, interrupt_handle_0x7e);
    set_idt_entry(0x7f, interrupt_handle_0x7f);
    set_idt_entry(0x80, interrupt_handle_0x80);
    set_idt_entry(0x81, interrupt_handle_0x81);
    set_idt_entry(0x82, interrupt_handle_0x82);
    set_idt_entry(0x83, interrupt_handle_0x83);
    set_idt_entry(0x84, interrupt_handle_0x84);
    set_idt_entry(0x85, interrupt_handle_0x85);
    set_idt_entry(0x86, interrupt_handle_0x86);
    set_idt_entry(0x87, interrupt_handle_0x87);
    set_idt_entry(0x88, interrupt_handle_0x88);
    set_idt_entry(0x89, interrupt_handle_0x89);
    set_idt_entry(0x8a, interrupt_handle_0x8a);
    set_idt_entry(0x8b, interrupt_handle_0x8b);
    set_idt_entry(0x8c, interrupt_handle_0x8c);
    set_idt_entry(0x8d, interrupt_handle_0x8d);
    set_idt_entry(0x8e, interrupt_handle_0x8e);
    set_idt_entry(0x8f, interrupt_handle_0x8f);
    set_idt_entry(0x90, interrupt_handle_0x90);
    set_idt_entry(0x91, interrupt_handle_0x91);
    set_idt_entry(0x92, interrupt_handle_0x92);
    set_idt_entry(0x93, interrupt_handle_0x93);
    set_idt_entry(0x94, interrupt_handle_0x94);
    set_idt_entry(0x95, interrupt_handle_0x95);
    set_idt_entry(0x96, interrupt_handle_0x96);
    set_idt_entry(0x97, interrupt_handle_0x97);
    set_idt_entry(0x98, interrupt_handle_0x98);
    set_idt_entry(0x99, interrupt_handle_0x99);
    set_idt_entry(0x9a, interrupt_handle_0x9a);
    set_idt_entry(0x9b, interrupt_handle_0x9b);
    set_idt_entry(0x9c, interrupt_handle_0x9c);
    set_idt_entry(0x9d, interrupt_handle_0x9d);
    set_idt_entry(0x9e, interrupt_handle_0x9e);
    set_idt_entry(0x9f, interrupt_handle_0x9f);
    set_idt_entry(0xa0, interrupt_handle_0xa0);
    set_idt_entry(0xa1, interrupt_handle_0xa1);
    set_idt_entry(0xa2, interrupt_handle_0xa2);
    set_idt_entry(0xa3, interrupt_handle_0xa3);
    set_idt_entry(0xa4, interrupt_handle_0xa4);
    set_idt_entry(0xa5, interrupt_handle_0xa5);
    set_idt_entry(0xa6, interrupt_handle_0xa6);
    set_idt_entry(0xa7, interrupt_handle_0xa7);
    set_idt_entry(0xa8, interrupt_handle_0xa8);
    set_idt_entry(0xa9, interrupt_handle_0xa9);
    set_idt_entry(0xaa, interrupt_handle_0xaa);
    set_idt_entry(0xab, interrupt_handle_0xab);
    set_idt_entry(0xac, interrupt_handle_0xac);
    set_idt_entry(0xad, interrupt_handle_0xad);
    set_idt_entry(0xae, interrupt_handle_0xae);
    set_idt_entry(0xaf, interrupt_handle_0xaf);
    set_idt_entry(0xb0, interrupt_handle_0xb0);
    set_idt_entry(0xb1, interrupt_handle_0xb1);
    set_idt_entry(0xb2, interrupt_handle_0xb2);
    set_idt_entry(0xb3, interrupt_handle_0xb3);
    set_idt_entry(0xb4, interrupt_handle_0xb4);
    set_idt_entry(0xb5, interrupt_handle_0xb5);
    set_idt_entry(0xb6, interrupt_handle_0xb6);
    set_idt_entry(0xb7, interrupt_handle_0xb7);
    set_idt_entry(0xb8, interrupt_handle_0xb8);
    set_idt_entry(0xb9, interrupt_handle_0xb9);
    set_idt_entry(0xba, interrupt_handle_0xba);
    set_idt_entry(0xbb, interrupt_handle_0xbb);
    set_idt_entry(0xbc, interrupt_handle_0xbc);
    set_idt_entry(0xbd, interrupt_handle_0xbd);
    set_idt_entry(0xbe, interrupt_handle_0xbe);
    set_idt_entry(0xbf, interrupt_handle_0xbf);
    set_idt_entry(0xc0, interrupt_handle_0xc0);
    set_idt_entry(0xc1, interrupt_handle_0xc1);
    set_idt_entry(0xc2, interrupt_handle_0xc2);
    set_idt_entry(0xc3, interrupt_handle_0xc3);
    set_idt_entry(0xc4, interrupt_handle_0xc4);
    set_idt_entry(0xc5, interrupt_handle_0xc5);
    set_idt_entry(0xc6, interrupt_handle_0xc6);
    set_idt_entry(0xc7, interrupt_handle_0xc7);
    set_idt_entry(0xc8, interrupt_handle_0xc8);
    set_idt_entry(0xc9, interrupt_handle_0xc9);
    set_idt_entry(0xca, interrupt_handle_0xca);
    set_idt_entry(0xcb, interrupt_handle_0xcb);
    set_idt_entry(0xcc, interrupt_handle_0xcc);
    set_idt_entry(0xcd, interrupt_handle_0xcd);
    set_idt_entry(0xce, interrupt_handle_0xce);
    set_idt_entry(0xcf, interrupt_handle_0xcf);
    set_idt_entry(0xd0, interrupt_handle_0xd0);
    set_idt_entry(0xd1, interrupt_handle_0xd1);
    set_idt_entry(0xd2, interrupt_handle_0xd2);
    set_idt_entry(0xd3, interrupt_handle_0xd3);
    set_idt_entry(0xd4, interrupt_handle_0xd4);
    set_idt_entry(0xd5, interrupt_handle_0xd5);
    set_idt_entry(0xd6, interrupt_handle_0xd6);
    set_idt_entry(0xd7, interrupt_handle_0xd7);
    set_idt_entry(0xd8, interrupt_handle_0xd8);
    set_idt_entry(0xd9, interrupt_handle_0xd9);
    set_idt_entry(0xda, interrupt_handle_0xda);
    set_idt_entry(0xdb, interrupt_handle_0xdb);
    set_idt_entry(0xdc, interrupt_handle_0xdc);
    set_idt_entry(0xdd, interrupt_handle_0xdd);
    set_idt_entry(0xde, interrupt_handle_0xde);
    set_idt_entry(0xdf, interrupt_handle_0xdf);
    set_idt_entry(0xe0, interrupt_handle_0xe0);
    set_idt_entry(0xe1, interrupt_handle_0xe1);
    set_idt_entry(0xe2, interrupt_handle_0xe2);
    set_idt_entry(0xe3, interrupt_handle_0xe3);
    set_idt_entry(0xe4, interrupt_handle_0xe4);
    set_idt_entry(0xe5, interrupt_handle_0xe5);
    set_idt_entry(0xe6, interrupt_handle_0xe6);
    set_idt_entry(0xe7, interrupt_handle_0xe7);
    set_idt_entry(0xe8, interrupt_handle_0xe8);
    set_idt_entry(0xe9, interrupt_handle_0xe9);
    set_idt_entry(0xea, interrupt_handle_0xea);
    set_idt_entry(0xeb, interrupt_handle_0xeb);
    set_idt_entry(0xec, interrupt_handle_0xec);
    set_idt_entry(0xed, interrupt_handle_0xed);
    set_idt_entry(0xee, interrupt_handle_0xee);
    set_idt_entry(0xef, interrupt_handle_0xef);
    set_idt_entry(0xf0, interrupt_handle_0xf0);
    set_idt_entry(0xf1, interrupt_handle_0xf1);
    set_idt_entry(0xf2, interrupt_handle_0xf2);
    set_idt_entry(0xf3, interrupt_handle_0xf3);
    set_idt_entry(0xf4, interrupt_handle_0xf4);
    set_idt_entry(0xf5, interrupt_handle_0xf5);
    set_idt_entry(0xf6, interrupt_handle_0xf6);
    set_idt_entry(0xf7, interrupt_handle_0xf7);
    set_idt_entry(0xf8, interrupt_handle_0xf8);
    set_idt_entry(0xf9, interrupt_handle_0xf9);
    set_idt_entry(0xfa, interrupt_handle_0xfa);
    set_idt_entry(0xfb, interrupt_handle_0xfb);
    set_idt_entry(0xfc, interrupt_handle_0xfc);
    set_idt_entry(0xfd, interrupt_handle_0xfd);
    set_idt_entry(0xfe, interrupt_handle_0xfe);
    set_idt_entry(0xff, interrupt_handle_0xff);
    asm volatile ("lidt %0" : : "m" (m_idt));
}
