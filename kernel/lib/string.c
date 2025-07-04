
#include <stddef.h>
#include <stdint.h>
#include <cpuid.h>
#include <debug/log.h>

#include "defs.h"

void* memset(void* s, int c, size_t n) {
    // NOTE: we assume that fast short rep stosb is supported, meaning that
    //       0-128 length strings should be fast
    void* d = s;
    asm volatile (
        "rep stosb"
        : "+D"(s), "+c"(n)
        : "a"((unsigned char)c)
        : "memory"
    );
    return d;
}

__attribute__((always_inline))
static inline void __rep_movsb(void* dest, const void* src, size_t n) {
    asm volatile (
        "rep movsb"
        : "+D"(dest), "+S"(src), "+c"(n)
        :
        : "memory"
    );
}

void* memcpy(void* restrict dest, const void* restrict src, size_t n) {
    // fast path for zero length
    // TODO: patch away if we have fast zero-length rep movsb
    if (UNLIKELY(n == 0))
        return dest;

    __rep_movsb(dest, src, n);
    return dest;
}

void* memmove(void* dest, const void* src, size_t n) {
    // fast path for zero length or the same exact buffer
    if (UNLIKELY(n == 0) || (dest == src)) {
        return dest;
    }

    void* d = dest;

    if (src < dest && dest < src + n) {
        // calculate the backwards copy size
        size_t tail_size = dest - src;

        // we are going to copy backwards at a chunk size of the non-overlapping
        // area, the tail size is always smaller than n at the start, so we can
        // use a do-while, and we are starting from that area
        dest += n - tail_size;
        src += n - tail_size;
        do {
            // perform the forward copy
            __rep_movsb(dest, src, tail_size);

            // and now move the pointers and size backwards
            dest -= tail_size;
            src -= tail_size;
            n -= tail_size;
        } while (n > tail_size);

        // if we are left with non-zero size then we
        // just need to perform this one last copy
        if (n != 0) {
            __rep_movsb(dest, src, n);
        }
    } else {
        // not overlapping, use a normal copy
        __rep_movsb(dest, src, n);
    }

    return d;
}

int memcmp(const void* vl, const void* vr, size_t n) {
    const unsigned char* l = vl;
    const unsigned char* r = vr;
    for (; n && *l == *r; n--, l++, r++) {
    }
    return n ? *l - *r : 0;
}

size_t strlen(const char* s) {
    const char *a = s;
    for (; *s; s++) {}
    return s - a;
}

int strcmp(const char* l, const char* r) {
    for (; *l == *r && *l; l++, r++) {}
    return *(unsigned char *)l - *(unsigned char *)r;
}

void string_verify_features(void) {
    uint32_t eax, ebx, ecx, edx;

    __cpuid_count(7, 0, eax, ebx, ecx, edx);
    if ((ebx & bit_ENH_MOVSB) == 0) WARN("string: Missing enhanced REP MOVSB/STOSB");
    if ((edx & BIT4) == 0) WARN("string: Missing fast short REP MOVSB");

    __cpuid_count(7, 1, eax, ebx, ecx, edx);
    // if ((eax & BIT10) == 0) LOG_WARN("string: Missing zero-length REP MOVSB");
    if ((eax & BIT11) == 0) WARN("string: Missing fast short REP STOSB");
    // if ((eax & BIT12) == 0) LOG_WARN("string: Missing fast short REP CMPSB/CSASB");
}
