
#include <debug/log.h>
#include <lib/string.h>
#include <mem/alloc.h>
#include <mem/memory.h>
#include <mem/phys.h>
#include <mem/virt.h>
#include <tomatodotnet/host.h>

void tdn_host_log_trace(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    debug_vprint("[*] ", "\n", format, ap);
    va_end(ap);
}

void tdn_host_log_warn(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    debug_vprint("[!] ", "\n", format, ap);
    va_end(ap);
}

void tdn_host_log_error(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    debug_vprint("[-] ", "\n", format, ap);
    va_end(ap);
}

void tdn_host_printf(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    debug_vprint("", "", format, ap);
    va_end(ap);
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Resolving assemblies
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool tdn_host_resolve_assembly(const char* name, uint16_t revision, tdn_file_t* out_file) {
    return false;
}

tdn_err_t tdn_host_read_file(void* file, size_t offset, size_t size, void* buffer) {
    return TDN_ERROR_CHECK_FAILED;
}

void tdn_host_close_file(tdn_file_t file) {
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Jit memory management
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
// Just normal allocator
//----------------------------------------------------------------------------------------------------------------------

void* tdn_host_mallocz(size_t size, size_t align) {
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

//----------------------------------------------------------------------------------------------------------------------
// Mapping memory for the jit
//----------------------------------------------------------------------------------------------------------------------

/**
 * Jit region, in the -2gb region to allow the code
 * to access the jit builtin functions using relative accesses
 */
static void* m_jit_watermark = (void*)JIT_ADDR;

void* tdn_host_jit_alloc(size_t size) {
    if ((UINTPTR_MAX - (uintptr_t)m_jit_watermark) < size) {
        return NULL;
    }

    // get the pointer
    void* ptr = m_jit_watermark;

    // allocate the range
    if (IS_ERROR(virt_alloc_range((uintptr_t)ptr, SIZE_TO_PAGES(size)))) {
        return NULL;
    }

    // increment the watermark
    m_jit_watermark += ALIGN_UP(size, PAGE_SIZE);

    return ptr;
}

void tdn_host_jit_set_exec(void* ptr, size_t size) {
    virt_remap_range((uintptr_t)ptr, SIZE_TO_PAGES(size), MAP_PERM_X);
}

//----------------------------------------------------------------------------------------------------------------------
// TODO: Dumping spidir
//----------------------------------------------------------------------------------------------------------------------

void* tdn_host_jit_start_dump(void) {
    return NULL;
}

void tdn_host_jit_end_dump(void* ctx) {
}

spidir_dump_status_t tdn_host_jit_dump_callback(const char* data, size_t size, void* ctx) {
    return SPIDIR_DUMP_STOP;
}
