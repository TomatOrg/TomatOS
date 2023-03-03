#include "gdb.h"
#include "sync/spinlock.h"
#include "arch/apic.h"
#include "irq/irq.h"
#include "arch/intrin.h"
#include "thread/scheduler.h"
#include "util/string.h"
#include "util/stb_ds.h"
#include "arch/gdt.h"
#include "mem/vmm.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Serial driver
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static uint8_t m_gdb_irq = 0;

#define BAUD_LOW_OFFSET    0x00
#define BAUD_HIGH_OFFSET   0x01
#define IER_OFFSET         0x01
#define LCR_SHADOW_OFFSET  0x01
#define FCR_SHADOW_OFFSET  0x02
#define IR_CONTROL_OFFSET  0x02
#define FCR_OFFSET         0x02
#define EIR_OFFSET         0x02
#define BSR_OFFSET         0x03
#define LCR_OFFSET         0x03
#define MCR_OFFSET         0x04
#define LSR_OFFSET         0x05
#define MSR_OFFSET         0x06

#define LSR_TXRDY  0x20
#define LSR_RXDA   0x01
#define DLAB       0x01
#define MCR_DTRC   0x01
#define MCR_RTS    0x02
#define MSR_CTS    0x10
#define MSR_DSR    0x20
#define MSR_RI     0x40
#define MSR_DCD    0x80

#define UART_BASE       0x3f8
#define UART_BPS        115200
#define UART_DATA       8
#define UART_STOP       1
#define UART_PARITY     0
#define UART_BREAK_SET  0

INTERRUPT static int serial_try_read() {
    uint8_t status = __inbyte(UART_BASE + LSR_OFFSET);
    if ((status & LSR_RXDA) == 0) {
        return -1;
    }

    return __inbyte(UART_BASE);
}

INTERRUPT static char serial_read() {
    int c;
    do {
        c = serial_try_read();
    } while (c == -1);
    return (char)c;
}

INTERRUPT static void serial_write(char b) {
    uint8_t status;
    do {
        status = __inbyte(UART_BASE + LSR_OFFSET);
    } while ((status & LSR_TXRDY) == 0);

    __outbyte(UART_BASE, b);
}

static void serial_init() {
    uint8_t data = UART_DATA - 5;
    size_t divisor = 115200 / UART_BPS;
    uint8_t output_data = (DLAB << 7) | (UART_BREAK_SET << 6) | (UART_PARITY << 3) | (UART_STOP << 2) | data;
    __outbyte(UART_BASE + LCR_OFFSET, output_data);
    __outbyte(UART_BASE + BAUD_HIGH_OFFSET, divisor >> 8);
    __outbyte(UART_BASE + BAUD_LOW_OFFSET, (uint8_t)divisor);
    output_data = (UART_BREAK_SET << 6) | (UART_PARITY << 3) | (UART_STOP << 2) | data;
    __outbyte(UART_BASE + LCR_OFFSET, output_data);
}

void gdb_enter() {
    __asm__ (
        "pushq %rax\n"
        "pushfq\n"
        "popq %rax\n"
        "orq $0x0100, %rax\n"
        "pushq %rax\n"
        "popfq\n"
        "popq %rax\n"
    );
}

static void gdb_irq() {
    while (true) {
        irq_wait(m_gdb_irq);

        int c;
        while ((c = serial_try_read()) != -1) {
            if (c == 3) {
                gdb_enter();
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static uint8_t m_send_checksum = 0;

INTERRUPT static void gdb_send_start() {
    m_send_checksum = 0;
    serial_write('$');
}

INTERRUPT static void gdb_send(const char* payload) {
    while (*payload) {
        serial_write(*payload);
        m_send_checksum += *payload;
        payload++;
    }
}

INTERRUPT static void gdb_send_hex(uintptr_t hex) {
    char buffer[17];
    snprintf(buffer, sizeof(buffer), "%lX", hex);
    gdb_send(buffer);
}

INTERRUPT static void gdb_send_end() {
    // send the checksum
    serial_write('#');
    serial_write("0123456789ABCDEF"[m_send_checksum >> 4]);
    serial_write("0123456789ABCDEF"[m_send_checksum & 0xF]);

    while (true) {
        char c = serial_read();
        if (c == '+') {
            return;
        } else if (c == '-') {
            ASSERT(!"gdb could not get the packet");
        }
    }
}

INTERRUPT static void gdb_send_packet(const char* payload) {
    while (true) {
        // send the start marker
        serial_write('$');

        // send the data
        uint8_t checksum = 0;
        while (*payload) {
            serial_write(*payload);
            checksum += *payload;
            payload++;
        }

        // send the checksum
        serial_write('#');
        serial_write("0123456789ABCDEF"[checksum >> 4]);
        serial_write("0123456789ABCDEF"[checksum & 0xF]);

        while (true) {
            char c = serial_read();
            if (c == '+') {
                return;
            } else if (c == '-') {
                break;
            }
        }
    }
}

static char m_gdb_command[2048];

INTERRUPT static int hex_char_to_int(char c) {
    if ('0' <= c && c <= '9') {
        return c - '0';
    } else if ('a' <= c && c <= 'f') {
        return (c - 'a') + 10;
    } else if ('A' <= c && c <= 'F') {
        return (c - 'A') + 10;
    } else {
        return -1;
    }
}

INTERRUPT static uintptr_t hex_to_int(char* ptr, char** out, int max) {
    uintptr_t value = 0;
    while (*ptr && max--) {
        int c = hex_char_to_int(*ptr);
        if (c == -1) {
            break;
        }
        ptr++;
        value <<= 4;
        value |= c;
    }
    if (out) *out = ptr;
    return value;
}

INTERRUPT static char* receive_gdb_packet() {
retry_from_start:
    // wait for the start marker
    char c;
    do {
        c = serial_read();
    } while (c != '$');

    // we got it, try to read the data
retry:
    uint8_t checksum = 0;
    size_t length = 0;
    while(true) {
        c = serial_read();
        if (c == '#') {
            break;
        } else if (c == '$') {
            goto retry;
        }

        m_gdb_command[length] = c;
        length++;
        ASSERT(length <= sizeof(m_gdb_command));

        checksum += c;
    }
    m_gdb_command[length] = '\0';

    // read the checksum
    char checksum_raw[3];
    checksum_raw[0] = serial_read();
    if (checksum_raw[0] == '$') goto retry;
    checksum_raw[1] = serial_read();
    if (checksum_raw[1] == '$') goto retry;
    checksum_raw[2] = '\0';

    // verify it
    char* ptr = checksum_raw;
    uint8_t should_be_checksum = hex_to_int(ptr, NULL, 2);
    if (should_be_checksum == checksum) {
        // success
        serial_write('+');
    } else {
        // failed, request again
        serial_write('-');
        goto retry_from_start;
    }

    return m_gdb_command;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define GDB_EINVAL      0x16
#define GDB_ESRCH       0x03
#define GDB_EFAULT      0x0e

static void gdb_send_error(int code) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "E%02X", code);
    gdb_send_packet(buffer);
}

static void gdb_send_unknown(char* packet) {
    TRACE("gdb: unknown packet: `%s`", packet);
    gdb_send_packet("");
}

typedef enum gdb_signal {
    GDB_SIGILL = 4,
    GDB_SIGTRAP = 5,
    GDB_SIGEMT = 7,
    GDB_SIGFPE = 8,
    GDB_SIGSEGV = 11,
} gdb_signal_t;

INTERRUPT static void gdb_send_stop_reply(gdb_signal_t signal) {
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "T%02Xthread:%X;",
             signal, get_current_thread()->id);
    gdb_send_packet(buffer);
}

static spinlock_t m_gdb_enter_lock = INIT_SPINLOCK();

static thread_t* find_thread_by_id(int id) {
    for (int i = 0; i < arrlen(g_all_threads); i++) {
        if (g_all_threads[i]->id == id) {
            return g_all_threads[i];
        }
    }
    return NULL;
}

static thread_t* m_selected_thread = NULL;

static bool match_string(char* in, const char* match, char** out) {
    while (*match) {
        if (*in != *match) {
            return false;
        }
        match++;
        in++;
    }

    if (out != NULL) {
        *out = in;
    }

    return true;
}

INTERRUPT void gdb_handle_exception(exception_context_t* ctx) {
    reset_trace_lock();

    if (m_gdb_irq == 0) {
        return;
    }

    spinlock_lock(&m_gdb_enter_lock);

    // first of all stop all the cores so nothing is running but us
    lapic_stop_all_cores();

    // set the current thread that was interrupted
    thread_t* current_thread = get_current_thread();
    m_selected_thread = current_thread;
    save_thread_exception_context(current_thread, ctx);

    // turn off the single steeping
    ctx->rflags.TF = 0;

    // figure the trap type
    gdb_signal_t trap;
    switch (ctx->int_num) {
        case EXCEPTION_DIVIDE_ERROR: trap = GDB_SIGFPE; break;
        case EXCEPTION_DEBUG: trap = GDB_SIGTRAP; break;
        case EXCEPTION_BREAKPOINT: trap = GDB_SIGTRAP; break;
        case EXCEPTION_INVALID_OPCODE: trap = GDB_SIGILL; break;
        case EXCEPTION_GP_FAULT: trap = GDB_SIGSEGV; break;
        case EXCEPTION_PAGE_FAULT: trap = GDB_SIGSEGV; break;
        case EXCEPTION_FP_ERROR: trap = GDB_SIGFPE; break;
        case EXCEPTION_SIMD: trap = GDB_SIGFPE; break;
        default: trap = GDB_SIGTRAP; break;
    }

    // TODO: check if any hardware breakpoints were triggered and if so
    //       pass it as more parameters to gdb_send_stop_reply

    gdb_send_stop_reply(trap);

    while (true) {
        char* packet = receive_gdb_packet();

        switch (*packet++) {
            case '?': {
                gdb_send_stop_reply(trap);
            } break;

            case 'C': {
                // we don't actually have signals so if this
                // happens just tell it that we got it again
                gdb_send_stop_reply(GDB_SIGSEGV);
            } break;

            case 'g': {
                #define R64 "%016lX"
                #define R32 "%08lX"
                char buffer[1024];
                uint64_t rax = __builtin_bswap64(m_selected_thread->save_state.rax);
                uint64_t rbx = __builtin_bswap64(m_selected_thread->save_state.rbx);
                uint64_t rcx = __builtin_bswap64(m_selected_thread->save_state.rcx);
                uint64_t rdx = __builtin_bswap64(m_selected_thread->save_state.rdx);
                uint64_t rsi = __builtin_bswap64(m_selected_thread->save_state.rsi);
                uint64_t rdi = __builtin_bswap64(m_selected_thread->save_state.rdi);
                uint64_t rbp = __builtin_bswap64(m_selected_thread->save_state.rbp);
                uint64_t rsp = __builtin_bswap64(m_selected_thread->save_state.rsp);
                uint64_t r8 = __builtin_bswap64(m_selected_thread->save_state.r8);
                uint64_t r9 = __builtin_bswap64(m_selected_thread->save_state.r9);
                uint64_t r10 = __builtin_bswap64(m_selected_thread->save_state.r10);
                uint64_t r11 = __builtin_bswap64(m_selected_thread->save_state.r11);
                uint64_t r12 = __builtin_bswap64(m_selected_thread->save_state.r12);
                uint64_t r13 = __builtin_bswap64(m_selected_thread->save_state.r13);
                uint64_t r14 = __builtin_bswap64(m_selected_thread->save_state.r14);
                uint64_t r15 = __builtin_bswap64(m_selected_thread->save_state.r15);
                uint64_t rip = __builtin_bswap64(m_selected_thread->save_state.rip);
                uint32_t rflags = __builtin_bswap32(m_selected_thread->save_state.rflags.packed);
                uint32_t gdt_code = __builtin_bswap32(GDT_CODE);
                uint32_t gdt_data = __builtin_bswap32(GDT_DATA);
                snprintf(buffer, sizeof(buffer),
                         R64 R64 R64 R64 R64 R64 R64 R64
                         R64 R64 R64 R64 R64 R64 R64 R64
                         R64 R32
                         R32 R32 R32 R32 R32,
                         rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp,
                         r8, r9, r10, r11, r12, r13, r14, r15,
                         rip, rflags,
                         gdt_code, gdt_data, gdt_data, gdt_data, gdt_data, gdt_data);
                gdb_send_packet(buffer);
                #undef R64
                #undef R32
            } break;

            case 'm': {
                uintptr_t addr = hex_to_int(packet, &packet, 16);
                ASSERT(*packet++ == ',');
                uintptr_t length = hex_to_int(packet, NULL, 16);
                if (length <= 2048 && vmm_is_mapped(addr, length)) {
                    char buffer[4096 + 1];
                    int i;
                    for (i = 0; i < length; i++) {
                        uint8_t value = *(uint8_t*)(addr + i);
                        buffer[i * 2 + 0] = "0123456789ABCDEF"[value >> 4];
                        buffer[i * 2 + 1] = "0123456789ABCDEF"[value & 0xF];
                    }
                    buffer[i * 2] = '\0';
                    gdb_send_packet(buffer);
                } else {
                    gdb_send_error(GDB_EFAULT);
                }
            } break;

            case 'H': {
                switch (*packet++) {
                    case 'g': {
                        int id = (int)hex_to_int(packet, NULL, 16);
                        if (id == 0) {
                            // pick any thread, stay with the one we broke on
                            m_selected_thread = current_thread;
                            gdb_send_packet("OK");
                        } else {
                            // we got a real id, find it and set it
                            thread_t* selected = find_thread_by_id(id);
                            if (selected != NULL) {
                                m_selected_thread = selected;
                                gdb_send_packet("OK");
                            } else {
                                TRACE("gdb: could not select thread %d, not found", id);
                                gdb_send_error(GDB_ESRCH);
                            }
                        }
                    } break;

                    case 'c': {
                        // make sure we do this for all cores
                        if (*packet == '-') {
                            packet++;
                            ASSERT(*packet++ == '1');
                        } else {
//                            int thread_id = hex_to_int(packet, NULL, 16);
                            // TODO: what do we do with this
//                            ASSERT(thread_id == 0);
                        }
                        gdb_send_packet("OK");
                    } break;

                    default: {
                        gdb_send_unknown(packet - 2);
                    } break;
                }
            } break;

            case 'q': {
                if (match_string(packet, "Supported", &packet)) {
                    ASSERT(*packet++ == ':');

                    // TODO: if we need anything use this code
//                    while (*packet) {
//                        if (strcmp(packet, "") == 0) {
//                            //
//                        } else {
//                            // find the end of the attribute since we don't know it
//                            while (*packet && *packet != ';') packet++;
//                            if (*packet == ';') packet++;
//                        }
//                    }

                    // send back our packet size
                    gdb_send_packet("PacketSize=2048;qXfer:threads:read+;qXfer:libraries:read+");

                } else if (match_string(packet, "Xfer:threads:read", &packet)) {
                    ASSERT(*packet++ == ':');
                    ASSERT(*packet++ == ':');

                    gdb_send_start();
                    gdb_send("l<?xml version=\"1.0\"?>");
                    gdb_send("<threads>");
                    for (int i = 0; i < arrlen(g_all_threads); i++) {
                        gdb_send("<thread ");
                        gdb_send("id=\"");
                        gdb_send_hex(g_all_threads[i]->id);
                        // TODO: core number
                        gdb_send("\" name=\"");
                        gdb_send(g_all_threads[i]->name);
                        gdb_send("\">");
                        switch (get_thread_status(g_all_threads[i])) {
                            case THREAD_STATUS_RUNNING: gdb_send("Running"); break;
                            case THREAD_STATUS_RUNNABLE: gdb_send("Runnable"); break;
                            case THREAD_STATUS_WAITING: gdb_send("Waiting"); break;
                            case THREAD_STATUS_IDLE: gdb_send("Idle"); break;
                            default: break;
                        }
                        gdb_send("</thread>");
                    }
                    gdb_send("</threads>");
                    gdb_send_end();

                } else if (match_string(packet, "Xfer:libraries:read", &packet)) {
                    ASSERT(*packet++ == ':');
                    ASSERT(*packet++ == ':');

                    TRACE("REQUESTED LIBRARIES");

                    gdb_send_start();
                    gdb_send("<library-list>");
                    gdb_send("<library name=\"symbols.so\">");
                    gdb_send("</library>");
                    gdb_send("</library-list>");
                    gdb_send_end();


                } else if (match_string(packet, "Attached", NULL)) {
                    gdb_send_packet("1");

//                } else if (match_string(packet, "fThreadInfo", NULL)) {
//                    // Obtain a list of all active thread IDs from the target
//
//                    char thread_list_buffer[2048];
//                    char *current = thread_list_buffer;
//                    *current++ = 'm';
//                    size_t left = sizeof(thread_list_buffer) - 2;
//                    for (int i = 0; i < arrlen(g_all_threads); i++) {
//                        size_t got = snprintf(current, left, "%X,", g_all_threads[i]->id);
//                        left -= got;
//                        ASSERT(left >= 0);
//                        current += got;
//                    }
//                    current[-1] = 'l';
//                    current[0] = '\0';
//
//                    gdb_send_packet(thread_list_buffer);
//
//                } else if (match_string(packet, "sThreadInfo", NULL)) {
//                    gdb_send_packet("l");

                } else if (match_string(packet, "Offsets", NULL)) {
                    gdb_send_packet("Text=0;Data=0;Bss=0");

//                } else if (match_string(packet, "ThreadExtraInfo", &packet)) {
//                    ASSERT(*packet++ == ',');
//                    uintptr_t thread_id = hex_to_int(packet, NULL, 16);
//                    thread_t* thread = find_thread_by_id((int)thread_id);
//                    if (thread != NULL) {
//                        char buffer[2048 + 1];
//                        char* name = thread->name;
//                        int i;
//                        for (i = 0; *name; i++) {
//                            uint8_t value = *name++;
//                            buffer[i * 2 + 0] = "0123456789ABCDEF"[value >> 4];
//                            buffer[i * 2 + 1] = "0123456789ABCDEF"[value & 0xF];
//                        }
//                        buffer[i * 2] = '\0';
//                        gdb_send_packet(buffer);
//                    } else {
//                        TRACE("gdb: could not get thread info fo %d, not found", thread_id);
//                        gdb_send_error(GDB_ESRCH);
//                    }

                } else if (*packet == 'C') {
                    char buffer[64];
                    snprintf(buffer, sizeof(buffer), "QC%X", m_selected_thread->id);
                    gdb_send_packet(buffer);

                } else {
                    gdb_send_unknown(packet - 1);
                }
            } break;

            // unknown packet, send an empty response
            default: {
                gdb_send_unknown(packet - 1);
            } break;
        }
    }

cleanup:
    restore_thread_exception_context(current_thread, ctx);

    // if we are coming from an exception then don't actually
    // continue, just die
    if (ctx == NULL) {
        lapic_resume_all_cores();
    }

    spinlock_unlock(&m_gdb_enter_lock);
}

err_t init_gdb() {
    err_t err = NO_ERROR;

    TRACE("Initializing gdb stub");

    serial_init();

    CHECK_AND_RETHROW(alloc_irq(1, &m_gdb_irq));

    thread_t* thread = create_thread(gdb_irq, NULL, "gdb/serial-irq");
    CHECK_ERROR(thread != NULL, ERROR_OUT_OF_MEMORY);
    scheduler_ready_thread(thread);

cleanup:
    return err;
}
