#include <util/elf64.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

typedef struct symbol {
    uint32_t address;
    size_t size;
    const char* name;
} symbol_t;

static symbol_t* m_symbols;
static size_t m_symbols_len, m_symbols_capacity;

static int find_symbol_insert_index(uintptr_t address) {
    int start = 0;
    int end = m_symbols_len - 1;

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

static void increase_size() {
    m_symbols_len++;
    if (m_symbols_len == m_symbols_capacity) {
        m_symbols_capacity += m_symbols_capacity / 2;
        m_symbols = realloc(m_symbols, m_symbols_capacity * sizeof(symbol_t));
    }
}

static void insert_symbol(symbol_t symbol) {
    int idx = find_symbol_insert_index(symbol.address);
    if (idx != -1) {
        increase_size();
        if (idx < m_symbols_len) {
            memmove(&m_symbols[idx + 1], &m_symbols[idx], sizeof(symbol_t) * (m_symbols_len - idx - 1));
            m_symbols[idx] = symbol;
        } else {
            m_symbols[m_symbols_len - 1] = symbol;
        }
    }
}

void debug_load_symbols() {
    struct stat stat;
    int kernfd = open("out/bin/tomatos.elf", O_RDONLY);
    fstat(kernfd, &stat);
    void* kernel = mmap(NULL, stat.st_size, PROT_READ, MAP_PRIVATE, kernfd, 0);
    Elf64_Ehdr* ehdr = kernel;
    Elf64_Shdr* symtab = NULL;

    // get the tables we need
    Elf64_Shdr* sections = kernel + ehdr->e_shoff;
    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (sections[i].sh_type == SHT_SYMTAB) {
            symtab = &sections[i];
            break;
        }
    }

    // get the strtab of the symtab
    char* strtab = kernel + sections[symtab->sh_link].sh_offset;

    // load all the symbols into an array we can easily search
    Elf64_Sym* symbols = kernel + symtab->sh_offset;
    for (int i = 0; i < symtab->sh_size / sizeof(Elf64_Sym); i++) {
        insert_symbol((symbol_t){
            .address = (uint32_t)symbols[i].st_value,
            .size = symbols[i].st_size,
            .name = strtab + symbols[i].st_name
        });
    }
}

const char* debug_get_name(uintptr_t addr) {
    if (m_symbols_len == 0) {
        return NULL;
    }

    int l = 0;
    int r = m_symbols_len;
    while (l <= r) {
        int m = l + (r - l) / 2;

        // found the exact symbol
        if (m_symbols[m].address <= addr && addr < m_symbols[m].address + m_symbols[m].size) {
            return m_symbols[m].name;
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

typedef struct stack_ent {
    const char* func_name;
    uint32_t time;
} stack_ent_t;

int main(void) {
    m_symbols_capacity = 4;
    m_symbols = malloc(m_symbols_capacity * sizeof(symbol_t));
    m_symbols_len = 0;

    debug_load_symbols();

    stack_ent_t* stack = malloc(sizeof(stack_ent_t) * 256);
    int stack_idx = 0;

    struct stat stat;
    int dumpfd = open("profiler.trace", O_RDONLY);
    fstat(dumpfd, &stat);
    uint32_t* dump = mmap(NULL, stat.st_size, PROT_READ, MAP_PRIVATE, dumpfd, 0);

    int buffer_size = 256 * 1024;
    int max_line_size = 32 * 1024;
    char* buffer = malloc(buffer_size); 
    int buffer_idx = 0;

    int outfd = open("profiler.trace.txt", O_CREAT | O_WRONLY);
    for (size_t i = 0; i < stat.st_size / 4;) {
        uint32_t firstbyte = dump[i++];
        if (firstbyte >> 31) { // function entry
            stack[stack_idx++] = (stack_ent_t) {
                .func_name = debug_get_name(firstbyte),
                .time = dump[i++]
            };
        } else { // function exit
            for (int j = 0; j < stack_idx; j++) {
                int len = strlen(stack[j].func_name);
                memcpy(buffer + buffer_idx, stack[j].func_name, len);
                buffer_idx += len;
                buffer[buffer_idx++] = (j != (stack_idx - 1)) ? ';' : ' ';
            }
            buffer_idx += sprintf(buffer + buffer_idx, "%zu\n", firstbyte - stack[stack_idx - 1].time);

            // overestimate: assume a line will take up to max_line_size
            if (buffer_idx >= (buffer_size - max_line_size)) {
                write(outfd, buffer, buffer_idx);
                buffer_idx = 0;
            }

            stack_idx--;
        }
    } 
}