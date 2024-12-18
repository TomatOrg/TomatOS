#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct symbol {
    uintptr_t address;
    size_t size;
    const char* name;
} symbol_t;

/**
 * Load symbols from the kernel binary
 */
void debug_load_symbols(void);

/**
 * Create a new symbol
 */
void debug_create_symbol(const char* name, uintptr_t addr, size_t size);

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