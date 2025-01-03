
#include <stddef.h>
#include <stdint.h>
#include <arch/cpuid.h>
#include <debug/log.h>

#include "defs.h"

void* memset(void* s, int c, size_t n) {
    // fast path for zero length
    if (n == 0) return s;

    void* d = s;
    asm volatile (
        "rep stosb"
        : "+D"(s), "+c"(n)
        : "a"((unsigned char)c)
        : "memory"
    );
    return d;
}

void* memcpy(void* restrict dest, const void* restrict src, size_t n) {
    // fast path for zero length
    if (n == 0) return dest;

    void* d = dest;
    asm volatile (
        "rep movsb"
        : "+D"(dest), "+S"(src), "+c"(n)
        :
        : "memory"
    );
    return d;
}

void* memmove(void* dest, const void* src, size_t n) {
    // fast path for zero length or the same exact buffer
    if (n == 0 || (dest == src)) {
        return dest;
    }

    if (dest < src) {
        return memcpy(dest, src, n);
    }

    // perform a normal slow backwards copy
    char* d = dest;
    const char* s = src;
    while (n--) {
        d[n] = s[n];
    }

    return dest;
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
    __cpuid(7, 0, eax, ebx, ecx, edx);

    if ((ebx & BIT9) == 0) LOG_WARN("string: Missing enhanced REP MOVSB/STOSB");
    if ((edx & BIT4) == 0) LOG_WARN("string: Missing fast short REP MOVSB");

    // __cpuid(7, 1, eax, ebx, ecx, edx);
    // if ((eax & BIT10) == 0) LOG_WARN("string: Missing zero-length REP MOVSB");
    // if ((eax & BIT12) == 0) LOG_WARN("string: Missing fast short REP CMPSB/CSASB");
}
