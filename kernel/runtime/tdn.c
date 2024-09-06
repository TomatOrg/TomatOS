
#include <debug/log.h>
#include <lib/string.h>
#include <mem/alloc.h>
#include <tomatodotnet/host.h>

void tdn_host_log_trace(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    log_vprintf("[*] ", format, ap);
    va_end(ap);
}

void tdn_host_log_warn(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    log_vprintf("[!] ", format, ap);
    va_end(ap);
}

void tdn_host_log_error(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    log_vprintf("[-] ", format, ap);
    va_end(ap);
}

void tdn_host_printf(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    debug_vprintf(format, ap);
    va_end(ap);
}

void* tdn_host_mallocz(size_t size) {
    void* ptr = mem_alloc(size);
    if (ptr != NULL) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void* tdn_host_realloc(void* ptr, size_t size) {
    return mem_realloc(ptr, size);
}

void tdn_host_free(void* ptr) {
    if (ptr == NULL) return;
    mem_free(ptr);
}

const char* tdn_host_error_to_string(int error) {
    return "<unknown>";
}

void* memchr(const void *src, int c, size_t n) {
    const unsigned char* s = src;
    c = (unsigned char)c;
    for (; n && *s != c; s++, n--) {}
    return n ? (void *)s : 0;
}

size_t tdn_host_strnlen(const char* s, size_t n) {
    const char *p = memchr(s, 0, n);
    return p ? p - s : n;
}

void tdn_host_close_file(tdn_file_t file) {
}
