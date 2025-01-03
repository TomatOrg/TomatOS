#pragma once

#include <stdarg.h>

void init_early_logging();

void debug_printf(const char* fmt, ...);
void debug_vprintf(const char* fmt, va_list ap);

void log_vprintf(const char* prefix, const char* fmt, va_list ap);
void log_vprintf_nonewline(const char* prefix, const char* fmt, va_list ap);

// the actual logging function
#define __LOG(fmt, ...) \
    do { \
        debug_printf(fmt, ##__VA_ARGS__); \
    } while (0)

// log levels
#define LOG_DEBUG(fmt, ...)         __LOG("[*] " fmt "\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)          __LOG("[+] " fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)          __LOG("[!] " fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)         __LOG("[-] " fmt "\n", ##__VA_ARGS__)
#define LOG_CRITICAL(fmt, ...)      __LOG("[~] " fmt "\n", ##__VA_ARGS__)

// default log level
#define LOG(fmt, ...)               LOG_INFO(fmt, ##__VA_ARGS__)
