#pragma once

#ifndef __JETBRAINS_IDE__
#include "cpp_magic.h"
#else
#define _IF(...) __VA_ARGS__
#define IF(...) _IF
#endif

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// General purpose definitions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef __clang__
#define ALIGN_UP(x, align) __builtin_align_up(x, align)
#define ALIGN_DOWN(x, align) __builtin_align_down(x, align)
#else
#define ALIGN_UP(x, align) ({ \
    __typeof(x) _x = x; \
    __typeof(align) _align = align; \
    __typeof(_x) _result = (__typeof(_x))((_x + _align - 1) & ~(_align - 1)); \
    _result; \
})

#define ALIGN_DOWN(x, align) ({ \
    __typeof(x) _x = x; \
    __typeof(align) _align = align; \
    __typeof(_x) _result = (__typeof(_x))(((uintptr_t)_x) & ~(_align - 1)); \
    _result; \
})
#endif
#define SIGN_EXTEND(x, size) \
    ({ \
        struct { signed long long value : size; } s; \
        s.value = x; \
        s.value; \
    })

#define MIN(a, b) \
    ({ \
        __typeof(a) _a = (a); \
        __typeof(b) _b = (b); \
        _a < _b ? _a : _b; \
    })

#define MAX(a, b) \
    ({ \
        __typeof(a) _a = (a); \
        __typeof(b) _b = (b); \
        _a > _b ? _a : _b; \
    })

#define SAFE_INC(var) \
    ({ \
        __typeof(var) __old = var; \
        CHECK (!__builtin_add_overflow(var, 1, &var)); \
        __old; \
    })

#define SAFE_ADD(var, value) \
    ({ \
        __typeof(var) __result;\
        CHECK (!__builtin_add_overflow((var), (value), &__result)); \
        __result; \
    })

#define PACKED __attribute__((packed))
#define UNUSED __attribute__((unused))

#define SIGNATURE_16(A, B)        ((A) | (B << 8))
#define SIGNATURE_32(A, B, C, D)  (SIGNATURE_16 (A, B) | (SIGNATURE_16 (C, D) << 16))
#define SIGNATURE_64(A, B, C, D, E, F, G, H) (SIGNATURE_32 (A, B, C, D) | ((uint64_t) (SIGNATURE_32 (E, F, G, H)) << 32))

#define ARRAY_LEN(Array) (sizeof (Array) / sizeof ((Array)[0]))

#define STATIC_ASSERT(x) _Static_assert(x, #x)

#define LIKELY(cond)        __builtin_expect ((cond), 0)
#define UNLIKELY(cond)      __builtin_expect ((cond), 1)

#define arrend(arr) \
    ({ \
        __typeof(arr) __arr = arr; \
        (__arr + arrlen(__arr)); \
    })

#define isprint(c) \
    ({ \
        char _c = c; \
        32 <= _c && _c <= 126; \
    })

static inline bool mask_read(_Atomic(size_t)* p, uint32_t id) {
    size_t word = id / (sizeof(size_t) * 8);
    size_t mask = 1 << (id % (sizeof(size_t) * 8));
    return (atomic_load(&p[word]) & mask) != 0;
}

static inline void mask_set(_Atomic(size_t)* p, uint32_t id) {
    size_t word = id / (sizeof(size_t) * 8);
    size_t mask = 1 << (id % (sizeof(size_t) * 8));
    atomic_fetch_or(&p[word], mask);
}

static inline void mask_clear(_Atomic(size_t)* p, uint32_t id) {
    size_t word = id / (sizeof(size_t) * 8);
    size_t mask = 1 << (id % (sizeof(size_t) * 8));
    atomic_fetch_and(&p[word], ~mask);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// useful constants
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define  BIT0     0x00000001
#define  BIT1     0x00000002
#define  BIT2     0x00000004
#define  BIT3     0x00000008
#define  BIT4     0x00000010
#define  BIT5     0x00000020
#define  BIT6     0x00000040
#define  BIT7     0x00000080
#define  BIT8     0x00000100
#define  BIT9     0x00000200
#define  BIT10    0x00000400
#define  BIT11    0x00000800
#define  BIT12    0x00001000
#define  BIT13    0x00002000
#define  BIT14    0x00004000
#define  BIT15    0x00008000
#define  BIT16    0x00010000
#define  BIT17    0x00020000
#define  BIT18    0x00040000
#define  BIT19    0x00080000
#define  BIT20    0x00100000
#define  BIT21    0x00200000
#define  BIT22    0x00400000
#define  BIT23    0x00800000
#define  BIT24    0x01000000
#define  BIT25    0x02000000
#define  BIT26    0x04000000
#define  BIT27    0x08000000
#define  BIT28    0x10000000
#define  BIT29    0x20000000
#define  BIT30    0x40000000
#define  BIT31    0x80000000
#define  BIT32    0x0000000100000000ULL
#define  BIT33    0x0000000200000000ULL
#define  BIT34    0x0000000400000000ULL
#define  BIT35    0x0000000800000000ULL
#define  BIT36    0x0000001000000000ULL
#define  BIT37    0x0000002000000000ULL
#define  BIT38    0x0000004000000000ULL
#define  BIT39    0x0000008000000000ULL
#define  BIT40    0x0000010000000000ULL
#define  BIT41    0x0000020000000000ULL
#define  BIT42    0x0000040000000000ULL
#define  BIT43    0x0000080000000000ULL
#define  BIT44    0x0000100000000000ULL
#define  BIT45    0x0000200000000000ULL
#define  BIT46    0x0000400000000000ULL
#define  BIT47    0x0000800000000000ULL
#define  BIT48    0x0001000000000000ULL
#define  BIT49    0x0002000000000000ULL
#define  BIT50    0x0004000000000000ULL
#define  BIT51    0x0008000000000000ULL
#define  BIT52    0x0010000000000000ULL
#define  BIT53    0x0020000000000000ULL
#define  BIT54    0x0040000000000000ULL
#define  BIT55    0x0080000000000000ULL
#define  BIT56    0x0100000000000000ULL
#define  BIT57    0x0200000000000000ULL
#define  BIT58    0x0400000000000000ULL
#define  BIT59    0x0800000000000000ULL
#define  BIT60    0x1000000000000000ULL
#define  BIT61    0x2000000000000000ULL
#define  BIT62    0x4000000000000000ULL
#define  BIT63    0x8000000000000000ULL

#define  SIZE_1KB    0x00000400
#define  SIZE_2KB    0x00000800
#define  SIZE_4KB    0x00001000
#define  SIZE_8KB    0x00002000
#define  SIZE_16KB   0x00004000
#define  SIZE_32KB   0x00008000
#define  SIZE_64KB   0x00010000
#define  SIZE_128KB  0x00020000
#define  SIZE_256KB  0x00040000
#define  SIZE_512KB  0x00080000
#define  SIZE_1MB    0x00100000
#define  SIZE_2MB    0x00200000
#define  SIZE_4MB    0x00400000
#define  SIZE_8MB    0x00800000
#define  SIZE_16MB   0x01000000
#define  SIZE_32MB   0x02000000
#define  SIZE_64MB   0x04000000
#define  SIZE_128MB  0x08000000
#define  SIZE_256MB  0x10000000
#define  SIZE_512MB  0x20000000
#define  SIZE_1GB    0x40000000
#define  SIZE_2GB    0x80000000
#define  SIZE_4GB    0x0000000100000000ULL
#define  SIZE_8GB    0x0000000200000000ULL
#define  SIZE_16GB   0x0000000400000000ULL
#define  SIZE_32GB   0x0000000800000000ULL
#define  SIZE_64GB   0x0000001000000000ULL
#define  SIZE_128GB  0x0000002000000000ULL
#define  SIZE_256GB  0x0000004000000000ULL
#define  SIZE_512GB  0x0000008000000000ULL
#define  SIZE_1TB    0x0000010000000000ULL
#define  SIZE_2TB    0x0000020000000000ULL
#define  SIZE_4TB    0x0000040000000000ULL
#define  SIZE_8TB    0x0000080000000000ULL
#define  SIZE_16TB   0x0000100000000000ULL
#define  SIZE_32TB   0x0000200000000000ULL
#define  SIZE_64TB   0x0000400000000000ULL
#define  SIZE_128TB  0x0000800000000000ULL
#define  SIZE_256TB  0x0001000000000000ULL
#define  SIZE_512TB  0x0002000000000000ULL
#define  SIZE_1PB    0x0004000000000000ULL
#define  SIZE_2PB    0x0008000000000000ULL
#define  SIZE_4PB    0x0010000000000000ULL
#define  SIZE_8PB    0x0020000000000000ULL
#define  SIZE_16PB   0x0040000000000000ULL
#define  SIZE_32PB   0x0080000000000000ULL
#define  SIZE_64PB   0x0100000000000000ULL
#define  SIZE_128PB  0x0200000000000000ULL
#define  SIZE_256PB  0x0400000000000000ULL
#define  SIZE_512PB  0x0800000000000000ULL
#define  SIZE_1EB    0x1000000000000000ULL
#define  SIZE_2EB    0x2000000000000000ULL
#define  SIZE_4EB    0x4000000000000000ULL
#define  SIZE_8EB    0x8000000000000000ULL

#define  BASE_1KB    0x00000400
#define  BASE_2KB    0x00000800
#define  BASE_4KB    0x00001000
#define  BASE_8KB    0x00002000
#define  BASE_16KB   0x00004000
#define  BASE_32KB   0x00008000
#define  BASE_64KB   0x00010000
#define  BASE_128KB  0x00020000
#define  BASE_256KB  0x00040000
#define  BASE_512KB  0x00080000
#define  BASE_1MB    0x00100000
#define  BASE_2MB    0x00200000
#define  BASE_4MB    0x00400000
#define  BASE_8MB    0x00800000
#define  BASE_16MB   0x01000000
#define  BASE_32MB   0x02000000
#define  BASE_64MB   0x04000000
#define  BASE_128MB  0x08000000
#define  BASE_256MB  0x10000000
#define  BASE_512MB  0x20000000
#define  BASE_1GB    0x40000000
#define  BASE_2GB    0x80000000
#define  BASE_4GB    0x0000000100000000ULL
#define  BASE_8GB    0x0000000200000000ULL
#define  BASE_16GB   0x0000000400000000ULL
#define  BASE_32GB   0x0000000800000000ULL
#define  BASE_64GB   0x0000001000000000ULL
#define  BASE_128GB  0x0000002000000000ULL
#define  BASE_256GB  0x0000004000000000ULL
#define  BASE_512GB  0x0000008000000000ULL
#define  BASE_1TB    0x0000010000000000ULL
#define  BASE_2TB    0x0000020000000000ULL
#define  BASE_4TB    0x0000040000000000ULL
#define  BASE_8TB    0x0000080000000000ULL
#define  BASE_16TB   0x0000100000000000ULL
#define  BASE_32TB   0x0000200000000000ULL
#define  BASE_64TB   0x0000400000000000ULL
#define  BASE_128TB  0x0000800000000000ULL
#define  BASE_256TB  0x0001000000000000ULL
#define  BASE_512TB  0x0002000000000000ULL
#define  BASE_1PB    0x0004000000000000ULL
#define  BASE_2PB    0x0008000000000000ULL
#define  BASE_4PB    0x0010000000000000ULL
#define  BASE_8PB    0x0020000000000000ULL
#define  BASE_16PB   0x0040000000000000ULL
#define  BASE_32PB   0x0080000000000000ULL
#define  BASE_64PB   0x0100000000000000ULL
#define  BASE_128PB  0x0200000000000000ULL
#define  BASE_256PB  0x0400000000000000ULL
#define  BASE_512PB  0x0800000000000000ULL
#define  BASE_1EB    0x1000000000000000ULL
#define  BASE_2EB    0x2000000000000000ULL
#define  BASE_4EB    0x4000000000000000ULL
#define  BASE_8EB    0x8000000000000000ULL
