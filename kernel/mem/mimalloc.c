#include "libc/sys/mman.h"
#include <stdlib.h>
#include <stdbool.h>
#include <thread/thread.h>
#include <mem/phys.h>
#include <assert.h>

THREAD_LOCAL int errno;

void _mi_stat_increase(void* stat, size_t amount) {}
void _mi_stat_decrease(void* stat, size_t amount) {}
void _mi_stat_counter_increase(void* stat, size_t amount) {}
void mi_stats_reset() {}
void _mi_options_init() {}
void _mi_stats_done() {}
void mi_stats_print(void* out) {}
void _mi_verbose_message(const char* fmt, ...) {}
void _mi_warning_message(const char* fmt, ...) {}
void _mi_error_message(const char* fmt, ...) {}

bool mi_option_is_enabled(int option) {
    return false;
}
long mi_option_get(int option) {
    return 0;
}
long mi_option_get_clamp(int option, long min, long max) {
    return min;
}
int mi_madvise(void* addr, size_t length, int advice) { return 0; }

void _mi_fputs(void* out, void* arg, const char* prefix, const char* message) {}

// TODO: i should implement this
void _mi_assert_fail(const char* assertion, const char* fname, unsigned int line, const char* func) {}


long _mi_clock_start() { return 0; }
long _mi_clock_end(long start) { return 0; }
long _mi_clock_now() { return 0; } // TODO: page expiration relies on it

size_t _mi_os_page_size(void) { return 4096; }

bool _mi_os_commit(void* p, size_t size, bool* is_zero, void* stats) { 
    mprotect(p, size, (PROT_READ | PROT_WRITE));
    if (is_zero != NULL) *is_zero = false; // TODO: optimize
    return true;
}
bool _mi_os_decommit(void* addr, size_t size, void* stats) { return true; }

_Atomic(size_t) _mi_numa_node_count = 1;
size_t _mi_os_numa_node_get(void) { return 0; }
size_t _mi_os_numa_node_count_get(void) { return 1; }

bool _mi_os_reset(void* addr, size_t size, void* tld_stats) { return true; }
bool _mi_os_unreset(void* addr, size_t size, bool* is_zero, void* tld_stats) { return true; }

bool _mi_os_protect(void* addr, size_t size) { return true; }
bool _mi_os_unprotect(void* addr, size_t size) { return true; }

size_t _mi_os_good_alloc_size(size_t size) { return ALIGN_UP(size, 4096); }

void _mi_os_init(void) {}

void* _mi_os_alloc_huge_os_pages(size_t pages, int numa_node, long max_msecs, size_t* pages_reserved, size_t* psize) {
    ASSERT("called _mi_os_alloc_huge_os_pages!");
    return NULL;
}
void  _mi_os_free_huge_pages(void* p, size_t size, void* stats) {
    ASSERT("called _mi_os_free_huge_pages!");
}

void* _mi_os_alloc_aligned(size_t size, size_t alignment, bool commit, bool* large, void* tld_stats) {
    void* addr = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (commit) mprotect(addr, size, PROT_READ | PROT_WRITE);
    *large = false;
    return addr;
}
void* _mi_os_alloc(size_t size, void* tld_stats) {
    bool large;
    return _mi_os_alloc_aligned(size, 4096, false, &large, NULL);
}

void _mi_os_free_ex(void* p, size_t size, bool was_committed, void* tld_stats) {}

void _mi_os_free(void* p, size_t size, void* stats) {}

