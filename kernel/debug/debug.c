#include "debug.h"
#include "util/stb_ds.h"
#include "kernel.h"

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

    symbol_t* symbol = debug_lookup_symbol(address);
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
        if (!opcodes--) {
            break;
        }
    }
}

static void insert_symbol(symbol_t symbol) {
    for (int i = 0; i < arrlen(m_symbols); i++) {
        if (m_symbols[i].address > symbol.address) {
            arrins(m_symbols, i, symbol);
            return;
        }
    }
    arrpush(m_symbols, symbol);
}

static char* strdup(const char* str) {
    int len = strlen(str);
    char* str2 = malloc(len + 1);
    strcpy(str2, str);
    return str2;
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
