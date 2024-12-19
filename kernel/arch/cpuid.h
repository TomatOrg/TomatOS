#pragma once

#include "intrin.h"

#define __cpuid(level, leaf, a, b, c, d) \
    do { \
        asm volatile ( \
            "cpuid" \
			: "=a" (a) \
            , "=b" (b) \
            , "=c" (c) \
            , "=d" (d) \
			: "0" (level), "2"(leaf)); \
    } while (0)

static inline INTRIN_ATTR unsigned int __get_cpuid_max(unsigned int ext, unsigned int* sig) {
    unsigned int eax, ebx, ecx, edx;
    __cpuid(ext, 0, eax, ebx, ecx, edx);
    if (sig)
        *sig = ebx;
    return eax;
}

#define CPUID_TIME_STAMP_COUNTER  0x15


