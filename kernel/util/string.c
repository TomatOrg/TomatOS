
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limits.h>

#define alias_load(T, p) \
    ({ \
        T value; \
        __builtin_memcpy(&value, p, sizeof(T)); \
        p += sizeof(T); \
        value; \
    })

#define alias_store(T, p, value) \
    do { \
        T __value = value; \
        __builtin_memcpy(p, &__value, sizeof(T)); \
        p += sizeof(T); \
    } while (0);

void* memcpy(void* restrict dest, const void* restrict src, size_t n) {
    unsigned char* curDest = dest;
    const unsigned char* curSrc = src;

    while(n >= 8 * 8) {
        uint64_t w1 = alias_load(uint64_t, curSrc);
        uint64_t w2 = alias_load(uint64_t, curSrc);
        uint64_t w3 = alias_load(uint64_t, curSrc);
        uint64_t w4 = alias_load(uint64_t, curSrc);
        uint64_t w5 = alias_load(uint64_t, curSrc);
        uint64_t w6 = alias_load(uint64_t, curSrc);
        uint64_t w7 = alias_load(uint64_t, curSrc);
        uint64_t w8 = alias_load(uint64_t, curSrc);
        alias_store(uint64_t, curDest, w1);
        alias_store(uint64_t, curDest, w2);
        alias_store(uint64_t, curDest, w3);
        alias_store(uint64_t, curDest, w4);
        alias_store(uint64_t, curDest, w5);
        alias_store(uint64_t, curDest, w6);
        alias_store(uint64_t, curDest, w7);
        alias_store(uint64_t, curDest, w8);
        n -= 8 * 8;
    }
    if(n >= 4 * 8) {
        uint64_t w1 = alias_load(uint64_t, curSrc);
        uint64_t w2 = alias_load(uint64_t, curSrc);
        uint64_t w3 = alias_load(uint64_t, curSrc);
        uint64_t w4 = alias_load(uint64_t, curSrc);
        alias_store(uint64_t, curDest, w1);
        alias_store(uint64_t, curDest, w2);
        alias_store(uint64_t, curDest, w3);
        alias_store(uint64_t, curDest, w4);
        n -= 4 * 8;
    }
    if(n >= 2 * 8) {
        uint64_t w1 = alias_load(uint64_t, curSrc);
        uint64_t w2 = alias_load(uint64_t, curSrc);
        alias_store(uint64_t, curDest, w1);
        alias_store(uint64_t, curDest, w2);
        n -= 2 * 8;
    }
    if(n >= 8) {
        uint64_t w = alias_load(uint64_t, curSrc);
        alias_store(uint64_t, curDest, w);
        n -= 8;
    }
    if(n >= 4) {
        uint32_t w = alias_load(uint32_t, curSrc);
        alias_store(uint32_t, curDest, w);
        n -= 4;
    }
    if(n >= 2) {
        uint16_t w = alias_load(uint16_t, curSrc);
        alias_store(uint16_t, curDest, w);
        n -= 2;
    }
    if(n)
        *curDest = *curSrc;
    return dest;
}

void* memset(void* dest, int val, size_t n) {
    unsigned char* curDest = dest;
    unsigned char byte = val;

    // Get rid of misalignment.
    while(n && ((uintptr_t)curDest & 7)) {
        *curDest++ = byte;
        --n;
    }

    uint64_t pattern64 = (
            (uint64_t)(byte)
            | ((uint64_t)(byte) << 8)
            | ((uint64_t)(byte) << 16)
            | ((uint64_t)(byte) << 24)
            | ((uint64_t)(byte) << 32)
            | ((uint64_t)(byte) << 40)
            | ((uint64_t)(byte) << 48)
            | ((uint64_t)(byte) << 56));

    uint32_t pattern32 = (
            (uint32_t)(byte)
            | ((uint32_t)(byte) << 8)
            | ((uint32_t)(byte) << 16)
            | ((uint32_t)(byte) << 24));

    uint16_t pattern16 = (
            (uint16_t)(byte)
            | ((uint16_t)(byte) << 8));

    while(n >= 8 * 8) {
        alias_store(uint64_t, curDest, pattern64);
        alias_store(uint64_t, curDest, pattern64);
        alias_store(uint64_t, curDest, pattern64);
        alias_store(uint64_t, curDest, pattern64);
        alias_store(uint64_t, curDest, pattern64);
        alias_store(uint64_t, curDest, pattern64);
        alias_store(uint64_t, curDest, pattern64);
        alias_store(uint64_t, curDest, pattern64);
        n -= 8 * 8;
    }
    if(n >= 4 * 8) {
        alias_store(uint64_t, curDest, pattern64);
        alias_store(uint64_t, curDest, pattern64);
        alias_store(uint64_t, curDest, pattern64);
        alias_store(uint64_t, curDest, pattern64);
        n -= 4 * 8;
    }
    if(n >= 2 * 8) {
        alias_store(uint64_t, curDest, pattern64);
        alias_store(uint64_t, curDest, pattern64);
        n -= 2 * 8;
    }
    if(n >= 8) {
        alias_store(uint64_t, curDest, pattern64);
        n -= 8;
    }
    if(n >= 4) {
        alias_store(uint32_t, curDest, pattern32);
        n -= 4;
    }
    if(n >= 2) {
        alias_store(uint16_t, curDest, pattern16);
        n -= 2;
    }
    if(n)
        *curDest = byte;
    return dest;
}

void* memmove (void* dest, const void* src, size_t len) {
    char *d = dest;
    const char *s = src;
    if (d < s) {
        memcpy(dest, src, len);
    } else {
        const char* lasts = s + (len - 1);
        char* lastd = d + (len - 1);
        while (len--) {
            *lastd-- = *lasts--;
        }
    }
    return dest;
}

int memcmp(const void* lhs, const void* rhs, size_t count) {
    const uint8_t* lhs_str = (const uint8_t*)lhs;
    const uint8_t* rhs_str = (const uint8_t*)rhs;
    for(size_t i = 0; i < count; i++) {
        if(lhs_str[i] < rhs_str[i]) {
            return -1;
        }
        if(lhs_str[i] > rhs_str[i]) {
            return 1;
        }
    }
    return 0;
}

int strcmp(const char *a, const char *b) {
    size_t i = 0;
    while(1) {
        unsigned char ac = a[i];
        unsigned char bc = b[i];
        if(!ac && !bc)
            return 0;
        if(ac < bc)
            return -1;
        if(ac > bc)
            return 1;
        i++;
    }
}

size_t strlen(const char *str) {
    size_t length = 0;
    while(*str++ != 0) {
        length++;
    }
    return length;
}

char* strcpy(char* restrict s1, const char* restrict s2) {
    register char *s = s1;
    while ( (*s++ = *s2++) != 0 );
    return s1;
}

static bool isspace(int c) {
    return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v';
}

static bool isdigit(int c) {
    return '0' <= c && c <= '9';
}

static bool isupper(int c) {
    return ('A' <= c && c <= 'Z');
}

static bool isalpha(int c) {
    return ('a' <= c && c <= 'z') || isupper(c);
}

unsigned long strtoul(const char* nptr, char** endptr, int base) {
    const char* s = nptr;
    bool neg = 0;
    char c = 0;

    unsigned long acc = 0;
    int any = 0;

    do {
        c = *s++;
    } while (isspace(c));

    if (c == '-') {
        neg = true;
        c = *s++;
    } else if (c == '+') {
        c = *s++;
    }

    if ((base == 0 || base == 16) && c == '0' && (*s == 'x' || *s == 'X')) {
        c = s[1];
        s += 2;
        base = 16;
    }

    if (base == 0) {
        base = c == '0' ? 8 : 10;
    }

    size_t cutoff = (size_t)ULONG_MAX / (size_t)base;
    size_t cutlim = (size_t)ULONG_MAX % (size_t)base;

    for (acc = 0, any = 0;; c = *s++) {
        if (isdigit(c)) {
            c -= '0';
        } else if (isalpha(c)) {
            c -= isupper(c) ? 'A' - 10 : 'a' - 10;
        } else {
            break;
        }

        if (c >= base) {
            break;
        }

        if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim)) {
            any = -1;
        } else {
            any = 1;
            acc *= base;
            acc += c;
        }
    }

    if (any < 0) {
        acc = ULONG_MAX;
    } else if (neg) {
        acc = -acc;
    }

    if (endptr != 0) {
        *endptr = (char *) (any ? s - 1 : nptr);
    }

    return acc;
}

int strncmp(const char* s1, const char* s2, size_t n ) {
    while (n && *s1 && (*s1 == *s2)) {
        ++s1;
        ++s2;
        --n;
    }
    if ( n == 0 ) {
        return 0;
    } else {
        return (*(unsigned char *)s1 - *(unsigned char *)s2);
    }
}

int tolower(int c) {
    if (isupper(c)) return c | 32;
    return c;
}

int strcasecmp(const char* s1, const char* s2) {
    while (*s1 && (tolower(*s1) == tolower(*s2))) {
        ++s1;
        ++s2;
    }
    return (*(unsigned char *)s1 - *(unsigned char *)s2);
}

char* strchr(const char* s, int c) {
    do {
        if (*s == c) {
            return (char*)s;
        }
    } while (*s++);
    return (0);
}

char* strcat(char* destination, const char* source) {
    char* ptr = destination + strlen(destination);
    while (*source != '\0') {
        *ptr++ = *source++;
    }
    *ptr = '\0';
    return destination;
}