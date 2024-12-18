#include "idt.h"

#include <mem/phys.h>
#include <mem/virt.h>
#include <sync/spinlock.h>

#include "apic.h"
#include "lib/defs.h"
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

/**
 * Forward declare the exception handler
 */
static void common_exception_handler(interrupt_frame_t* ctx, uint64_t int_num, uint64_t code);

#define EXCEPTION_STUB(num) \
    __attribute__((interrupt)) \
    static void exception_handler_##num(interrupt_frame_t* frame) { \
        common_exception_handler(frame, num, 0); \
    }

#define EXCEPTION_ERROR_STUB(num) \
    __attribute__((interrupt)) \
    static void exception_handler_##num(interrupt_frame_t* frame, uint64_t code) { \
        common_exception_handler(frame, num, 0); \
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
EXCEPTION_ERROR_STUB(0x0A);
EXCEPTION_ERROR_STUB(0x0B);
EXCEPTION_ERROR_STUB(0x0C);
EXCEPTION_ERROR_STUB(0x0D);
EXCEPTION_STUB(0x0F);
EXCEPTION_STUB(0x10);
EXCEPTION_ERROR_STUB(0x11);
EXCEPTION_STUB(0x12);
EXCEPTION_STUB(0x13);
EXCEPTION_STUB(0x14);
EXCEPTION_ERROR_STUB(0x15);
EXCEPTION_STUB(0x16);
EXCEPTION_STUB(0x17);
EXCEPTION_STUB(0x18);
EXCEPTION_STUB(0x19);
EXCEPTION_STUB(0x1A);
EXCEPTION_STUB(0x1B);
EXCEPTION_STUB(0x1C);
EXCEPTION_ERROR_STUB(0x1D);
EXCEPTION_ERROR_STUB(0x1E);
EXCEPTION_STUB(0x1F);

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

static void set_idt_entry(int vector, void* func, int ist, bool cli) {
    m_idt_entries[vector].handler_low = (uint16_t) ((uintptr_t)func & 0xFFFF);
    m_idt_entries[vector].handler_high = (uint64_t) ((uintptr_t)func >> 16);
    m_idt_entries[vector].gate_type = cli ? IDT_TYPE_INTERRUPT_32 : IDT_TYPE_TRAP_32;
    m_idt_entries[vector].selector = 8;
    m_idt_entries[vector].present = 1;
    m_idt_entries[vector].ring = 0;
    m_idt_entries[vector].ist = ist;
}

static const char* m_exception_names[32] = {
    [0x00] = "#DE - Division Error",
    [0x01] = "#DB - Debug",
    [0x02] = "Non-maskable Interrupt",
    [0x03] = "#BP - Breakpoint",
    [0x04] = "#OF - Overflow",
    [0x05] = "#BR - Bound Range Exceeded",
    [0x06] = "#UD - Invalid Opcode",
    [0x07] = "#NM - Device Not Available",
    [0x08] = "#DF - Double Fault",
    [0x09] = "Coprocessor Segment Overrun",
    [0x0A] = "#TS - Invalid TSS",
    [0x0B] = "#NP - Segment Not Present",
    [0x0C] = "#SS - Stack-Segment Fault",
    [0x0D] = "#GP - General Protection Fault",
    [0x0E] = "#PF - Page Fault",
    [0x10] = "#MF - x87 Floating-Point Exception",
    [0x11] = "#AC - Alignment Check",
    [0x12] = "#MC - Machine Check",
    [0x13] = "#XM/#XF - SIMD Floating-Point Exception",
    [0x14] = "#VE - Virtualization Exception",
    [0x15] = "#CP - Control Protection Exception",
    [0x1C] = "#HV - Hypervisor Injection Exception",
    [0x1D] = "#VC - VMM Communication Exception",
    [0x1E] = "#SX - Security Exception",
};

static spinlock_t m_exception_lock = INIT_SPINLOCK();

__attribute__((interrupt))
static void page_fault_handler(interrupt_frame_t* frame, uint64_t code) {
    if (virt_handle_page_fault(__readcr2())) {
        return;
    }

    common_exception_handler(frame, EXCEPT_IA32_PAGE_FAULT, code);
}

static void common_exception_handler(interrupt_frame_t* ctx, uint64_t int_num, uint64_t code) {
    spinlock_lock(&m_exception_lock);

    LOG_ERROR("---------------------------------------------------------------------------------");
    LOG_ERROR("");
    if (int_num < ARRAY_LENGTH(m_exception_names) && m_exception_names[int_num] != NULL) {
        LOG_ERROR("Got exception: %s", m_exception_names[int_num]);
    } else {
        LOG_ERROR("Got exception: #%d", int_num);
    }
    LOG_ERROR("");

    LOG_ERROR("Registers:");
    LOG_ERROR("RIP=%p RSP=%p", ctx->rip, ctx->rsp);
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
    set_idt_entry(EXCEPT_IA32_DIVIDE_ERROR, exception_handler_0x00, 0, true);
    set_idt_entry(EXCEPT_IA32_DEBUG, exception_handler_0x01, 0, true);
    set_idt_entry(EXCEPT_IA32_NMI, exception_handler_0x02, 2, true);
    set_idt_entry(EXCEPT_IA32_BREAKPOINT, exception_handler_0x03, 5, true);
    set_idt_entry(EXCEPT_IA32_OVERFLOW, exception_handler_0x04, 0, true);
    set_idt_entry(EXCEPT_IA32_BOUND, exception_handler_0x05, 0, true);
    set_idt_entry(EXCEPT_IA32_INVALID_OPCODE, exception_handler_0x06, 0, true);
    set_idt_entry(0x07, exception_handler_0x07, 0, true);
    set_idt_entry(EXCEPT_IA32_DOUBLE_FAULT, exception_handler_0x08, 3, true);
    set_idt_entry(0x09, exception_handler_0x09, 0, true);
    set_idt_entry(EXCEPT_IA32_INVALID_TSS, exception_handler_0x0A, 0, true);
    set_idt_entry(EXCEPT_IA32_SEG_NOT_PRESENT, exception_handler_0x0B, 0, true);
    set_idt_entry(EXCEPT_IA32_STACK_FAULT, exception_handler_0x0C, 0, true);
    set_idt_entry(EXCEPT_IA32_GP_FAULT, exception_handler_0x0D, 0, true);
    set_idt_entry(EXCEPT_IA32_PAGE_FAULT, page_fault_handler, 1, true);
    set_idt_entry(0x0F, exception_handler_0x0F, 0, true);
    set_idt_entry(EXCEPT_IA32_FP_ERROR, exception_handler_0x10, 0, true);
    set_idt_entry(EXCEPT_IA32_ALIGNMENT_CHECK, exception_handler_0x11, 0, true);
    set_idt_entry(EXCEPT_IA32_MACHINE_CHECK, exception_handler_0x12, 0, true);
    set_idt_entry(EXCEPT_IA32_SIMD, exception_handler_0x13, 0, true);
    set_idt_entry(0x14, exception_handler_0x14, 0, true);
    set_idt_entry(0x15, exception_handler_0x15, 0, true);
    set_idt_entry(0x16, exception_handler_0x16, 0, true);
    set_idt_entry(0x17, exception_handler_0x17, 0, true);
    set_idt_entry(0x18, exception_handler_0x18, 0, true);
    set_idt_entry(0x19, exception_handler_0x19, 0, true);
    set_idt_entry(0x1A, exception_handler_0x1A, 0, true);
    set_idt_entry(0x1B, exception_handler_0x1B, 0, true);
    set_idt_entry(0x1C, exception_handler_0x1C, 0, true);
    set_idt_entry(0x1D, exception_handler_0x1D, 0, true);
    set_idt_entry(0x1E, exception_handler_0x1E, 0, true);
    set_idt_entry(0x1F, exception_handler_0x1F, 0, true);
    asm volatile ("lidt %0" : : "m" (m_idt));
}
