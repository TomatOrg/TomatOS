#include "intr.h"

#include <debug/debug.h>
#include <mem/phys.h>
#include <mem/virt.h>
#include <sync/spinlock.h>
#include <thread/pcpu.h>
#include <thread/scheduler.h>
#include <time/tsc.h>

#include "apic.h"
#include "lib/defs.h"
#include "debug/log.h"
#include "intrin.h"
#include "regs.h"
#include "time/timer.h"

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

typedef struct intr {
    uint16_t limit;
    idt_entry_t* base;
} PACKED idt_t;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Exception handling - has a bunch of code to save registers so we can debug more easily
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct exception_frame {
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
} exception_frame_t;

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

typedef union selector_error_code {
    struct {
        uint32_t e : 1;
        uint32_t tbl : 2;
        uint32_t index : 13;
    };
    uint32_t packed;
} PACKED selector_error_code_t;

/**
 * Forward declare the exception handler
 */
void common_exception_handler(exception_frame_t* ctx);

#define EXCEPTION_STUB(num) \
    __attribute__((naked)) \
    static void exception_handler_##num() { \
        __asm__( \
            "pushq $0\n" \
            "pushq $" #num "\n" \
            "jmp common_exception_stub"); \
    }

#define EXCEPTION_ERROR_STUB(num) \
    __attribute__((naked)) \
    static void exception_handler_##num() { \
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
EXCEPTION_ERROR_STUB(0x0A);
EXCEPTION_ERROR_STUB(0x0B);
EXCEPTION_ERROR_STUB(0x0C);
EXCEPTION_ERROR_STUB(0x0D);
EXCEPTION_ERROR_STUB(0x0E);
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
 * Pretty print exception names
 */
static const char* m_exception_names[] = {
    "#DE - Division Error",
    "#DB - Debug",
    "Non-maskable Interrupt",
    "#BP - Breakpoint",
    "#OF - Overflow",
    "#BR - Bound Range Exceeded",
    "#UD - Invalid Opcode",
    "#NM - Device Not Available",
    "#DF - Double Fault",
    "Coprocessor Segment Overrun",
    "#TS - Invalid TSS",
    "#NP - Segment Not Present",
    "#SS - Stack-Segment Fault",
    "#GP - General Protection Fault",
    "#PF - Page Fault",
    "Reserved",
    "#MF - x87 Floating-Point Exception",
    "#AC - Alignment Check",
    "#MC - Machine Check",
    "#XM/#XF - SIMD Floating-Point Exception",
    "#VE - Virtualization Exception",
    "#CP - Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "#HV - Hypervisor Injection Exception",
    "#VC - VMM Communication Exception",
    "#SX - Security Exception",
    "Reserved",
};
STATIC_ASSERT(ARRAY_LENGTH(m_exception_names) == 32);

static spinlock_t m_exception_lock = SPINLOCK_INIT;

/**
 * The default exception handler, simply panics...
 */
static void default_exception_handler(exception_frame_t* ctx) {
    spinlock_acquire(&m_exception_lock);

    // reset the spinlock so we can print
    ERROR("");
    ERROR("****************************************************");
    ERROR("Exception occurred: %s (%lu)", m_exception_names[ctx->int_num], ctx->error_code);
    ERROR("****************************************************");
    ERROR("");

    page_fault_error_t page_fault_code = {};
    if (ctx->int_num == 0x0E) {
        page_fault_code = (page_fault_error_t) { .packed = ctx->error_code };
        if (page_fault_code.reserved_write) {
            ERROR("one or more page directory entries contain reserved bits which are set to 1");
        } else if (page_fault_code.instruction_fetch) {
            ERROR("tried to run non-executable code");
        } else {
            const char* rw = page_fault_code.write ? "write to" : "read from";
            if (!page_fault_code.present) {
                ERROR("%s non-present page", rw);
            } else {
                ERROR("page-protection violation when %s page", rw);
            }
        }
        ERROR("");
    } else if (ctx->int_num == 0x0D && ctx->error_code != 0) {
        selector_error_code_t selector = (selector_error_code_t) { .packed = ctx->error_code };
        static const char* table[] = {
            "GDT",
            "IDT",
            "LDT",
            "IDT"
        };
        ERROR("Accessing %s[%d]", table[selector.tbl], selector.index);
        ERROR("");
    }

    // check if we have threading_old already
    if (__rdmsr(MSR_IA32_FS_BASE) != 0) {
        thread_t* thread = scheduler_get_current_thread();
        if (thread != NULL) {
            ERROR("%p", thread);
            ERROR("Thread: `%.*s`", (int)sizeof(thread->name), thread->name);
        } else {
            ERROR("Thread: <none>");
        }
    }
    ERROR("CPU: #%d", get_cpu_id());
    ERROR("");

    // registers
    ERROR("RAX=%016lx RBX=%016lx RCX=%016lx RDX=%016lx", ctx->rax, ctx->rbx, ctx->rcx, ctx->rdx);
    ERROR("RSI=%016lx RDI=%016lx RBP=%016lx RSP=%016lx", ctx->rsi, ctx->rdi, ctx->rbp, ctx->rsp);
    ERROR("R8 =%016lx R9 =%016lx R10=%016lx R11=%016lx", ctx->r8 , ctx->r9 , ctx->r10, ctx->r11);
    ERROR("R12=%016lx R13=%016lx R14=%016lx R15=%016lx", ctx->r12, ctx->r13, ctx->r14, ctx->r15);
    ERROR("RIP=%016lx RFL=%lb", ctx->rip, ctx->rflags.packed);
    ERROR("FS =%016lx GS =%016x", __rdmsr(MSR_IA32_FS_BASE), 0); // TODO: GS BASE
    ERROR("CR0=%08lx CR2=%016lx CR3=%016lx CR4=%08lx", __readcr0(), __readcr2(), __readcr3(), __readcr4());

    ERROR("");

    // print the opcode for nicer debugging
    char buffer[256] = { 0 };
    debug_format_symbol(ctx->rip, buffer, sizeof(buffer));
    ERROR("Code: %s", buffer);
    // debug_disasm_at((void*)ctx->rip, 5);
    ERROR("");

    // stack trace
    ERROR("Stack trace:");
    size_t* base_ptr = (size_t*)ctx->rbp;

    int depth = 0;
    uintptr_t last_ret = 0;

    // if you want to print the assembly of a specific stack trace entry set this (start from 1)
    int to_print = 0;
    while (true) {
        if (!virt_is_mapped((uintptr_t)base_ptr)) {
            ERROR("\t%p is unmapped!", base_ptr);
            break;
        }

        size_t old_bp = base_ptr[0];
        size_t ret_addr = base_ptr[1];
        if (ret_addr == 0) {
            break;
        }

        if (last_ret == ret_addr) {
            depth++;
        } else {
            if (depth > 1) {
                ERROR("\t  ... repeating %d times", depth - 1);
            }

            last_ret = ret_addr;
            depth = 1;

            debug_format_symbol(ret_addr, buffer, sizeof(buffer));
            ERROR("\t> %s (0x%p)", buffer, (void*)ret_addr);

            to_print--;
            if (to_print == 0) {
                // debug_disasm_at((void*)ret_addr, 5);
            }
        }

        if (old_bp == 0) {
            break;
        } else if (old_bp <= (size_t)base_ptr) {
            ERROR("\tGoes back to %p", (void*)old_bp);
            break;
        }
        base_ptr = (size_t*)old_bp;
    }

    ERROR("");

    // stop
    ERROR("Halting :(");
    spinlock_release(&m_exception_lock);
    asm("hlt");
}

__attribute__((used))
static void common_exception_handler(exception_frame_t* ctx) {
    // special case for page
    if (ctx->int_num == EXCEPT_IA32_PAGE_FAULT) {
        if (virt_handle_page_fault(__readcr2())) {
            return;
        }
    }

    // no one handled it, panic
    default_exception_handler(ctx);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Scheduler interrupt
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

__attribute__((interrupt))
static void timer_interrupt_handler(interrupt_frame_t* frame) {
    lapic_eoi();

    // dispatch the timers, don't allow them to preempt
    // while we are doing so, if they did request it
    // then the preempt enable will handle it
    scheduler_preempt_disable();
    timer_dispatch();
    scheduler_preempt_enable();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////but it
// IDT setup
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * All interrupt handler entries
 */
static idt_entry_t m_idt_entries[256];

/**
 * Set a single idt entry
 */
static void set_idt_entry(int vector, void* func, int ist, bool cli) {
    m_idt_entries[vector].handler_low = (uint16_t) ((uintptr_t)func & 0xFFFF);
    m_idt_entries[vector].handler_high = (uint64_t) ((uintptr_t)func >> 16);
    m_idt_entries[vector].gate_type = cli ? IDT_TYPE_INTERRUPT_32 : IDT_TYPE_TRAP_32;
    m_idt_entries[vector].selector = 8;
    m_idt_entries[vector].present = 1;
    m_idt_entries[vector].ring = 0;
    m_idt_entries[vector].ist = ist;
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
    set_idt_entry(EXCEPT_IA32_PAGE_FAULT, exception_handler_0x0E, 1, true);
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
    set_idt_entry(0x20, timer_interrupt_handler, 0, true);

    idt_t idt = {
        .limit = sizeof(m_idt_entries) - 1,
        .base = m_idt_entries
    };
    asm volatile ("lidt %0" : : "m" (idt));
}
