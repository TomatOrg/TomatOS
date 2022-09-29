#pragma once

#include <stdint.h>
#include <stddef.h>

#include <dotnet/types.h>

typedef struct symbol {
    uintptr_t address;
    size_t size;
    const char* name;
} symbol_t;

/**
 * Disassemble and print to the debug output at the
 * given pointer the given amount of opcodes
 *
 * @param ptr       [IN] The start address
 * @param opcodes   [IN] The number of opcodes to bring
 */
void debug_disasm_at(void* ptr, int opcodes);

/**
 * Load symbols from the kernel binary
 */
void debug_load_symbols();

/**
 * Create a new symbol
 */
void debug_create_symbol(const char* name, uintptr_t addr, size_t size);

/**
 * Get the size of code by finding the first ret
 */
size_t debug_get_code_size(void* code);

/**
 * Lookup for a symbol, returns NULL if unknown
 */
symbol_t* debug_lookup_symbol(uintptr_t addr);

/**
 * Format the address into a symbol
 *
 * if invalid address just displays the address
 *
 * @param addr          [IN] The address
 * @param buffer        [IN] The destination buffer
 * @param buffer_size   [IN] The destination buffer size
 */
void debug_format_symbol(uintptr_t addr, char* buffer, size_t buffer_size);
