#include "idt.h"

#include <mem/virt.h>
#include <sync/spinlock.h>
#include <thread/scheduler.h>

#include "apic.h"
#include "lib/defs.h"
#include "time/timer.h"
#include "debug/log.h"
#include "intrin.h"
#include "regs.h"

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


#define EXCEPTION_STUB(num) \
    __attribute__((naked)) \
    static void interrupt_handle_##num() { \
        __asm__( \
            "pushq $0\n" \
            "pushq $" #num "\n" \
            "jmp common_exception_stub"); \
    }

#define EXCEPTION_ERROR_STUB(num) \
    __attribute__((naked)) \
    static void interrupt_handle_##num() { \
        __asm__( \
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
EXCEPTION_STUB(0x20);

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

static void set_idt_entry(int vector, void* func, int ist) {
    m_idt_entries[vector].handler_low = (uint16_t) ((uintptr_t)func & 0xFFFF);
    m_idt_entries[vector].handler_high = (uint64_t) ((uintptr_t)func >> 16);
    m_idt_entries[vector].gate_type = IDT_TYPE_INTERRUPT_32;
    m_idt_entries[vector].selector = 8;
    m_idt_entries[vector].present = 1;
    m_idt_entries[vector].ring = 0;
    m_idt_entries[vector].ist = ist;
}

static const char* m_exception_names[32] = {
    [EXCEPT_IA32_DIVIDE_ERROR] = "#DE - Divide Error",
    [EXCEPT_IA32_DEBUG] = "#DB - Debug",
    [EXCEPT_IA32_NMI] = "NMI Interrupt",
    [EXCEPT_IA32_BREAKPOINT] = "#BP - Breakpoint",
    [EXCEPT_IA32_OVERFLOW] = "#OF - Overflow",
    [EXCEPT_IA32_BOUND] = "#BR - BOUND Range Exceeded",
    [EXCEPT_IA32_INVALID_OPCODE] = "#UD - Invalid Opcode",
    [EXCEPT_IA32_DOUBLE_FAULT] = "#DF - Double Fault",
    [EXCEPT_IA32_INVALID_TSS] = "#TS - Invalid TSS",
    [EXCEPT_IA32_SEG_NOT_PRESENT] = "#NP - Segment Not Present",
    [EXCEPT_IA32_STACK_FAULT] = "#SS - Stack Fault Fault",
    [EXCEPT_IA32_GP_FAULT] = "#GP - General Protection",
    [EXCEPT_IA32_PAGE_FAULT] = "#PF - Page-Fault",
    [EXCEPT_IA32_FP_ERROR] = "#MF - x87 FPU Floating-Point Error",
    [EXCEPT_IA32_ALIGNMENT_CHECK] = "#AC - Alignment Check",
    [EXCEPT_IA32_MACHINE_CHECK] = "#MC - Machine-Check",
    [EXCEPT_IA32_SIMD] = "#XM - SIMD floating-point",
};

static spinlock_t m_exception_lock = INIT_SPINLOCK();

__attribute__((used))
void common_exception_handler(interrupt_context_t* ctx) {
    // call the scheduler if needed
    if (ctx->int_num == 0x20) {
        // ack the interrupt
        lapic_eoi();

        // run the scheduler
        scheduler_interrupt(ctx);

        return;
    }

    // attempt to handle page fault first
    if (ctx->int_num == EXCEPT_IA32_PAGE_FAULT) {
        if (virt_handle_page_fault(__readcr2())) {
            return;
        }
    }

    spinlock_lock(&m_exception_lock);

    LOG_ERROR("---------------------------------------------------------------------------------");
    LOG_ERROR("");
    if (ctx->int_num < ARRAY_LENGTH(m_exception_names) && m_exception_names[ctx->int_num] != NULL) {
        LOG_ERROR("Got exception: %s", m_exception_names[ctx->int_num]);
    } else {
        LOG_ERROR("Got exception: #%d", ctx->int_num);
    }
    LOG_ERROR("");

    LOG_ERROR("Registers:");
    LOG_ERROR("RAX=%p RBX=%p RCX=%p RDX=%p", ctx->rax, ctx->rbx, ctx->rcx, ctx->rdx);
    LOG_ERROR("RSI=%p RDI=%p RBP=%p RSP=%p", ctx->rsi, ctx->rdi, ctx->rbp, ctx->rsp);
    LOG_ERROR("R8 =%p R9 =%p R10=%p R11=%p", ctx->r8 , ctx->r9 , ctx->r10, ctx->r11);
    LOG_ERROR("R12=%p R13=%p R14=%p R15=%p", ctx->r12, ctx->r13, ctx->r14, ctx->r15);
    LOG_ERROR("RIP=%p", ctx->rip);
    LOG_ERROR("CR0=%08x CR2=%p CR3=%p CR4=%08x", __readcr0(), __readcr2(), __readcr3(), __readcr4());
    LOG_ERROR("");

    LOG_ERROR("Stack trace:");
    uintptr_t* frame_pointer = (uintptr_t*)ctx->rbp;
    for (;;) {
        uintptr_t return_address = *(frame_pointer + 1);
        if (return_address == 0) {
            break;
        }
        LOG_ERROR("\t%p", return_address);

        // make sure we won't go back up, otherwise we
        // have a loop potentially
        if ((*frame_pointer) >= (uintptr_t)frame_pointer) {
            break;
        }
        frame_pointer = (uintptr_t*)(*frame_pointer);
    }
    LOG_ERROR("");

    spinlock_unlock(&m_exception_lock);

    asm("cli");
    while(1)
        asm("hlt");
}

void init_idt() {
    //
    // IST usage:
    //  - 1: page fault
    //  - 2: nmi
    //  - 3: double fault
    //  - 4: scheduler
    //
    set_idt_entry(EXCEPT_IA32_DIVIDE_ERROR, interrupt_handle_0x00, 0);
    set_idt_entry(EXCEPT_IA32_DEBUG, interrupt_handle_0x01, 0);
    set_idt_entry(EXCEPT_IA32_NMI, interrupt_handle_0x02, 2);
    set_idt_entry(EXCEPT_IA32_BREAKPOINT, interrupt_handle_0x03, 0);
    set_idt_entry(EXCEPT_IA32_OVERFLOW, interrupt_handle_0x04, 0);
    set_idt_entry(EXCEPT_IA32_BOUND, interrupt_handle_0x05, 0);
    set_idt_entry(EXCEPT_IA32_INVALID_OPCODE, interrupt_handle_0x06, 0);
    set_idt_entry(0x7, interrupt_handle_0x07, 0);
    set_idt_entry(EXCEPT_IA32_DOUBLE_FAULT, interrupt_handle_0x08, 3);
    set_idt_entry(0x9, interrupt_handle_0x09, 0);
    set_idt_entry(EXCEPT_IA32_INVALID_TSS, interrupt_handle_0x0a, 0);
    set_idt_entry(EXCEPT_IA32_SEG_NOT_PRESENT, interrupt_handle_0x0b, 0);
    set_idt_entry(EXCEPT_IA32_STACK_FAULT, interrupt_handle_0x0c, 0);
    set_idt_entry(EXCEPT_IA32_GP_FAULT, interrupt_handle_0x0d, 0);
    set_idt_entry(EXCEPT_IA32_PAGE_FAULT, interrupt_handle_0x0e, 1);
    set_idt_entry(0xf, interrupt_handle_0x0f, 0);
    set_idt_entry(EXCEPT_IA32_FP_ERROR, interrupt_handle_0x10, 0);
    set_idt_entry(EXCEPT_IA32_ALIGNMENT_CHECK, interrupt_handle_0x11, 0);
    set_idt_entry(EXCEPT_IA32_MACHINE_CHECK, interrupt_handle_0x12, 0);
    set_idt_entry(EXCEPT_IA32_SIMD, interrupt_handle_0x13, 0);
    set_idt_entry(0x14, interrupt_handle_0x14, 0);
    set_idt_entry(0x15, interrupt_handle_0x15, 0);
    set_idt_entry(0x16, interrupt_handle_0x16, 0);
    set_idt_entry(0x17, interrupt_handle_0x17, 0);
    set_idt_entry(0x18, interrupt_handle_0x18, 0);
    set_idt_entry(0x19, interrupt_handle_0x19, 0);
    set_idt_entry(0x1a, interrupt_handle_0x1a, 0);
    set_idt_entry(0x1b, interrupt_handle_0x1b, 0);
    set_idt_entry(0x1c, interrupt_handle_0x1c, 0);
    set_idt_entry(0x1d, interrupt_handle_0x1d, 0);
    set_idt_entry(0x1e, interrupt_handle_0x1e, 0);
    set_idt_entry(0x1f, interrupt_handle_0x1f, 0);
    set_idt_entry(0x20, interrupt_handle_0x20, 4);
    asm volatile ("lidt %0" : : "m" (m_idt));
}

asm (
    ".global common_exception_stub\n"
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
