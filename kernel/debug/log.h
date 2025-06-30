#pragma once

#include <stdarg.h>

// log levels
#define DEBUG(fmt, ...)     debug_print("[?] " fmt "\n", ##__VA_ARGS__)
#define TRACE(fmt, ...)     debug_print("[*] " fmt "\n", ##__VA_ARGS__)
#define WARN(fmt, ...)      debug_print("[!] " fmt "\n", ##__VA_ARGS__)
#define ERROR(fmt, ...)     debug_print("[-] " fmt "\n", ##__VA_ARGS__)

/**
 * Early logging initialization
 */
void init_early_logging(void);

/**
 * logging initialization after allocator init
 */
void init_logging(void);

/**
 * Print a string to the debug log
 */
void debug_print(const char* fmt, ...) __attribute__((format(printf, (1), (2))));
void debug_vprint(const char* prefix, const char* suffix, const char* fmt, va_list va);
