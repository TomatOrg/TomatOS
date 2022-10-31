#include "libc/sys/mman.h"
#include "util/except.h"
#include <stdlib.h>
#include <stdbool.h>
#include <thread/thread.h>
#include <mem/phys.h>
#include <sync/mutex.h>
#include <mem/mem.h>
#include <assert.h>

THREAD_LOCAL int errno;
static uint64_t m_bump = OBJECT_HEAP_START;
static mutex_t m_bump_mutex = INIT_MUTEX();

static void commit_bump(uintptr_t ptr, size_t size) {
    err_t err = NO_ERROR;
    if (size != 0) {
        void *p = palloc(size);
        CHECK(p != NULL);
        CHECK_AND_RETHROW(vmm_map(DIRECT_TO_PHYS(p), (void*)ptr, size / PAGE_SIZE, MAP_WRITE | ((size == SIZE_2MB) ? MAP_LARGE : 0)));
    }
    cleanup:
    if (IS_ERROR(err)) {
        ERROR("Error while committing mimalloc memory");
    }
}

bool _mi_os_commit(void* p, size_t size, bool* is_zero, void* stats) { 
    if ((uint64_t)p >= OBJECT_HEAP_START && (uint64_t)p < OBJECT_HEAP_END) {
        uintptr_t p_start = (uintptr_t)p, p_end = (uintptr_t)(p + size);
        uintptr_t start = ALIGN_DOWN(p_start, PAGE_SIZE), end = ALIGN_UP(p_end, PAGE_SIZE);
        uintptr_t start_2mb = ALIGN_UP(p_start, SIZE_2MB), end_2mb = ALIGN_DOWN(p_end, SIZE_2MB);
        if (size >= (start_2mb - start)) {
            commit_bump(start, start_2mb - start);
            commit_bump(end_2mb, end - end_2mb);
            for (uintptr_t ptr = start_2mb; ptr < end_2mb; ptr += SIZE_2MB) commit_bump(ptr, SIZE_2MB);
        } else {
            commit_bump(start, size);
        }
    } 
    if (is_zero != NULL) *is_zero = true;
    return true;
}
void* _mi_os_alloc_aligned(size_t size, size_t alignment, bool commit, bool* large, void* tld_stats) {
    mutex_lock(&m_bump_mutex);
    m_bump = ALIGN_UP(m_bump, alignment);
    void *addr = (void*)m_bump;
    m_bump += size;
    mutex_unlock(&m_bump_mutex);

    bool is_zero;
    if (commit) _mi_os_commit(addr, size, &is_zero, tld_stats);
    *large = false; // we don't want to set this to true ever, because if large == true, mimalloc won't decommit
    return addr;
}


//////////// Stubs
void _mi_stat_increase(void* stat, size_t amount) {}
void _mi_stat_decrease(void* stat, size_t amount) {}
void _mi_stat_counter_increase(void* stat, size_t amount) {}
void mi_stats_reset() {}
void _mi_options_init() {}
void _mi_stats_done() {}
void mi_stats_print(void* out) {}

void _mi_verbose_message(const char* fmt, ...) {}
void _mi_warning_message(const char* fmt, ...) {}
void _mi_error_message(int err, const char* fmt, ...) {}
void _mi_assert_fail(const char* assertion, const char* fname, unsigned int line, const char* func) {}
void _mi_fputs(void* out, void* arg, const char* prefix, const char* message) {}

bool mi_option_is_enabled(int option) { return false; }
long mi_option_get(int option) { return 0; }
long mi_option_get_clamp(int option, long min, long max) { return min; }
int mi_madvise(void* addr, size_t length, int advice) { return 0; }

// TODO: i should implement this
long _mi_clock_start() { return 0; }
long _mi_clock_end(long start) { return 0; }
long _mi_clock_now() { return 0; } // TODO: page expiration relies on it

size_t _mi_os_page_size(void) { return 4096; }

bool _mi_os_decommit(void* addr, size_t size, void* stats) { return true; }

_Atomic(size_t) _mi_numa_node_count = 1;
int _mi_os_numa_node_get(void* tld) { return 0; }
size_t _mi_os_numa_node_count_get() { return 1; }

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
void* _mi_os_alloc(size_t size, void* tld_stats) {
    bool large;
    return _mi_os_alloc_aligned(size, 4096, false, &large, NULL);
}

void _mi_os_free_ex(void* p, size_t size, bool was_committed, void* tld_stats) {}

void _mi_os_free(void* p, size_t size, void* stats) {}

