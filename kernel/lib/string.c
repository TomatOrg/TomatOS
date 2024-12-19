
#include <stddef.h>
#include <stdint.h>
#include <arch/cpuid.h>
#include <debug/log.h>

#include "defs.h"

void* memset(void* s, int c, size_t n) {
    // fast path for zero length
    if (n == 0) return s;

    asm volatile (
        "rep stosb"
        :
        : "D"(s), "c"(n), "a"((unsigned char)c)
        : "memory"
    );
    return s;
}

void* memcpy(void* restrict dest, const void* restrict src, size_t n) {
    // fast path for zero length
    if (n == 0) return dest;

    asm volatile (
        "rep movsb"
        :
        : "D"(dest), "S"(src), "c"(n)
        : "memory"
    );
    return dest;
}

void* memmove(void* dest, const void* src, size_t n) {
    // fast path for zero length or the same exact buffer
    if (n == 0 || (dest == src)) {
        return dest;
    }

    if (dest < src || (char*)dest >= (char*)src + n) {
        // Non-overlapping or src is before dest: copy forward
        asm volatile (
            "rep movsb"
            :
            : "D"(dest), "S"(src), "c"(n)
            : "memory"
        );
    } else {
        const char *src_rear = (const char *)src + n;
        char *dest_rear = (char *)dest + n;
        size_t rear_size = (size_t)((char *)src + n - (char *)dest);

        // Copy rear chunks (non-overlapping region)
        asm volatile (
            "rep movsb"
            :
            : "D"(dest_rear - rear_size), "S"(src_rear - rear_size), "c"(rear_size)
            : "memory"
        );

        // Copy the rest (overlapping region)
        size_t overlap_size = n - rear_size;
        asm volatile (
            "rep movsb"
            :
            : "D"(dest), "S"(src), "c"(overlap_size)
            : "memory"
        );
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
