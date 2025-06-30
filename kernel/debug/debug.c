#include "debug.h"

#include <lib/elf64.h>
#include <lib/except.h>
#include <lib/string.h>
#include <mem/alloc.h>
#include <limine.h>
#include <lib/printf.h>

static symbol_t* m_symbols = NULL;
static int m_symbols_count = 0;

static void do_insert_symbol(int index, symbol_t symbol) {
    // increase the count
    m_symbols_count++;

    // reallocate the array
    symbol_t* ptr = mem_realloc(m_symbols, m_symbols_count * sizeof(symbol_t));
    ASSERT(ptr != NULL);

    // move the items after the element we want
    memmove(&ptr[index + 1], &ptr[index], (m_symbols_count - index - 1) * sizeof(symbol_t));

    // set the element
    ptr[index] = symbol;

    // save the new pointer
    m_symbols = ptr;
}

static int find_symbol_insert_index(uintptr_t address) {
    int start = 0;
    int end = m_symbols_count - 1;

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
        if (idx < m_symbols_count) {
            do_insert_symbol(idx, symbol);
        } else {
            do_insert_symbol(m_symbols_count, symbol);
        }
    }
}

static char* strdup(const char* str) {
    int len = strlen(str);
    char* str2 = mem_alloc(len + 1);
    memcpy(str2, str, len + 1);
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

/**
 * we need the elf so we can properly load the symbols
 */
extern struct limine_executable_file_request g_limine_executable_file_request;

void debug_load_symbols() {
    void* kernel = g_limine_executable_file_request.response->executable_file->address;

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

    TRACE("debug: Loaded %d symbols", m_symbols_count);
}

symbol_t* debug_lookup_symbol(uintptr_t addr) {
    if (m_symbols_count == 0) {
        return NULL;
    }

    int l = 0;
    int r = m_symbols_count;
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
        ksnprintf(buffer, buffer_size, "%016lx", addr);
    } else {
        ksnprintf(buffer, buffer_size, "%s+0x%03lx", sym->name, addr - sym->address);
    }
}
