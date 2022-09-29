#include "debug.h"
#include "util/stb_ds.h"
#include "kernel.h"
#include "mem/mem.h"
#include "dotnet/jit/jit.h"

#include <util/elf64.h>
#include <util/trace.h>

#include <Zydis/Zydis.h>

#include <stdbool.h>
#include <stdint.h>


static symbol_t* m_symbols;

static ZydisFormatterFunc default_print_address_absolute;

static ZyanStatus ZydisFormatterPrintAddressAbsolute(const ZydisFormatter* formatter, ZydisFormatterBuffer* buffer, ZydisFormatterContext* context) {
    ZyanU64 address;
    ZYAN_CHECK(ZydisCalcAbsoluteAddress(context->instruction, context->operand,
                                        context->runtime_address, &address));

    // look up the symbol
    symbol_t* symbol = NULL;
    if ((context->instruction->attributes & ZYDIS_ATTRIB_HAS_SEGMENT_GS) || (context->instruction->attributes & ZYDIS_ATTRIB_HAS_SEGMENT_FS)) {
        symbol = debug_lookup_symbol(address);
    } else {
        // check if in kernel
        if (KERNEL_BASE <= address && address < KERNEL_BASE + SIZE_8MB) {
            symbol = debug_lookup_symbol(address);
        }
    }

    if (symbol != NULL) {
        ZYAN_CHECK(ZydisFormatterBufferAppend(buffer, ZYDIS_TOKEN_SYMBOL));
        ZyanString* string;
        ZYAN_CHECK(ZydisFormatterBufferGetString(buffer, &string));

        char buffer[256] = { 0 };
        if (address - symbol->address == 0) {
            snprintf(buffer, sizeof(buffer), "%s", symbol->name);
        } else {
            snprintf(buffer, sizeof(buffer), "%s+%03x", symbol->name, address - symbol->address);
        }
        ZyanStringView view = (ZyanStringView){
            {
                0,
                {
                    ZYAN_NULL,
                    1,
                    0,
                    strlen(buffer) + 1,
                    sizeof(buffer),
                    sizeof(char),
                    ZYAN_NULL,
                    buffer
                }
            }
        };
        return ZyanStringAppend(string, &view);
    }

    return default_print_address_absolute(formatter, buffer, context);
}

size_t debug_get_code_size(void* ptr) {
    // initialize decoder and formatter
    ZydisDecoder decoder;
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);

    // decode and print it
    void* start = ptr;
    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE];
    while (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, ptr, UINT64_MAX,
                                               &instruction, operands, ZYDIS_MAX_OPERAND_COUNT_VISIBLE,
                                               ZYDIS_DFLAG_VISIBLE_OPERANDS_ONLY))) {

        // found the instruction we wanted, return it
        if (instruction.mnemonic == ZYDIS_MNEMONIC_RET) {
            return ptr - start;
        }

        // next...
        ptr += instruction.length;
    }

    return 0;
}

void debug_disasm_at(void* ptr, int opcodes) {
    // initialize decoder and formatter
    ZydisDecoder decoder;
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);

    ZydisFormatter formatter;
    ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);
    default_print_address_absolute = (ZydisFormatterFunc)&ZydisFormatterPrintAddressAbsolute;
    ZydisFormatterSetHook(&formatter, ZYDIS_FORMATTER_FUNC_PRINT_ADDRESS_ABS, (const void**)&default_print_address_absolute);

    // decode and print it
    bool first = true;
    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE];
    while (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, ptr, UINT64_MAX,
                                               &instruction, operands, ZYDIS_MAX_OPERAND_COUNT_VISIBLE,
                                               ZYDIS_DFLAG_VISIBLE_OPERANDS_ONLY))) {
        // format the opcode_info
        char buffer[256];
        ZydisFormatterFormatInstruction(&formatter, &instruction, operands,
                                        instruction.operand_count_visible, buffer, sizeof(buffer),
                                        (uintptr_t)ptr);

        // get the symbol name
        char addr_buffer[256] = { 0 };
        debug_format_symbol((uintptr_t)ptr, addr_buffer, sizeof(addr_buffer));

        // print it
        TRACE(" %c %s: %s", first ? '>' : ' ', addr_buffer, buffer);
        first = false;

        // next...
        ptr += instruction.length;
        if (--opcodes == 0) {
            break;
        }
    }
}

//void _MIR_dump_code (const char *name, int index, uint8_t *code, size_t _code_len) {
//    int64_t code_len = (int64_t)_code_len;
//    // initialize decoder and formatter
//    ZydisDecoder decoder;
//    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
//
//    ZydisFormatter formatter;
//    ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);
//    default_print_address_absolute = (ZydisFormatterFunc)&ZydisFormatterPrintAddressAbsolute;
//    ZydisFormatterSetHook(&formatter, ZYDIS_FORMATTER_FUNC_PRINT_ADDRESS_ABS, (const void**)&default_print_address_absolute);
//
//    // decode and print it
//    ZydisDecodedInstruction instruction;
//    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT_VISIBLE];
//    while (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, code, UINT64_MAX,
//                                               &instruction, operands, ZYDIS_MAX_OPERAND_COUNT_VISIBLE,
//                                               ZYDIS_DFLAG_VISIBLE_OPERANDS_ONLY))) {
//        // format the opcode_info
//        char buffer[256];
//        ZydisFormatterFormatInstruction(&formatter, &instruction, operands,
//                                        instruction.operand_count_visible, buffer, sizeof(buffer),
//                                        (uintptr_t)code);
//
//        // get the symbol name
//        char addr_buffer[256] = { 0 };
//        debug_format_symbol((uintptr_t)code, addr_buffer, sizeof(addr_buffer));
//
//        // print it
//        printf(" %s: %s\r\n", addr_buffer, buffer);
//
//        // next...
//        code += instruction.length;
//        code_len -= instruction.length;
//        if (code_len <= 0) {
//            break;
//        }
//    }
//}

static int find_symbol_insert_index(uintptr_t address) {
    int start = 0;
    int end = arrlen(m_symbols) - 1;

    while (start <= end) {
        int mid = (start + end) / 2;
        if (m_symbols[mid].address == address) {
            return -1;
        } else if (m_symbols[mid].address < address) {
            start = mid + 1;
        } else {
            end = mid - 1;
        }
    }

    return end + 1;
}

static void insert_symbol(symbol_t symbol) {
    int idx = find_symbol_insert_index(symbol.address);
    if (idx != -1) {
        if (idx < arrlen(m_symbols)) {
            arrins(m_symbols, idx, symbol);
        } else {
            arrpush(m_symbols, symbol);
        }
    }
}

static char* strdup(const char* str) {
    int len = strlen(str);
    char* str2 = malloc(len + 1);
    strcpy(str2, str);
    return str2;
}

void debug_create_symbol(const char* name, uintptr_t addr, size_t size) {
    // don't insert one if already exists
    if (debug_lookup_symbol(addr) != NULL) {
        return;
    }

    insert_symbol((symbol_t){
        .address = addr,
        .size = size,
        .name = strdup(name)
    });
}

void debug_load_symbols() {
    void* kernel = g_limine_kernel_file.response->kernel_file->address;

    Elf64_Ehdr* ehdr = kernel;
    Elf64_Shdr* symtab = NULL;

    if (ehdr->e_shoff == 0) {
        WARN("debug: kernel has no section table");
        return;
    }

    // get the tables we need
    Elf64_Shdr* sections = kernel + ehdr->e_shoff;
    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (sections[i].sh_type == SHT_SYMTAB) {
            symtab = &sections[i];
            break;
        }
    }
    if (symtab == NULL) {
        WARN("debug: kernel has no symbol table");
        return;
    }

    // get the strtab of the symtab
    char* strtab = kernel + sections[symtab->sh_link].sh_offset;

    // load all the symbols into an array we can easily search
    Elf64_Sym* symbols = kernel + symtab->sh_offset;
    for (int i = 0; i < symtab->sh_size / sizeof(Elf64_Sym); i++) {
        insert_symbol((symbol_t){
            .address = symbols[i].st_value,
            .size = symbols[i].st_size,
            .name = strdup(strtab + symbols[i].st_name)
        });
    }

    TRACE("debug: Loaded %d symbols", arrlen(m_symbols));
}

symbol_t* debug_lookup_symbol(uintptr_t addr) {
    if (arrlen(m_symbols) == 0) {
        return NULL;
    }

    int l = 0;
    int r = arrlen(m_symbols);
    while (l <= r) {
        int m = l + (r - l) / 2;

        // found the exact symbol
        if (m_symbols[m].address <= addr && addr < m_symbols[m].address + m_symbols[m].size) {
            return &m_symbols[m];
        }

        // continue searching
        if (m_symbols[m].address < addr) {
            l = m + 1;
        } else {
            r = m - 1;
        }
    }
    return NULL;
}

void debug_format_symbol(uintptr_t addr, char* buffer, size_t buffer_size) {
    symbol_t* sym = debug_lookup_symbol(addr);
    if (sym == NULL) {
        snprintf(buffer, buffer_size, "%016p", addr);
    } else {
        snprintf(buffer, buffer_size, "%s+0x%03lx", sym->name, addr - sym->address);
    }
}
