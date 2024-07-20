
#include <stddef.h>
#include <stdint.h>

void* memset(void* s, int c, size_t n) {
    for(size_t i = 0; i < n; i++) {
        ((char *)s)[i] = (char)c;
    }
    return s;
}

void* memcpy(void* restrict dest, const void* restrict src, size_t n) {
    for(size_t i = 0; i < n; i++) {
        ((char *)dest)[i] = ((const char *)src)[i];
    }
    return dest;
}

void* memmove(void* dest, const void* src, size_t n) {
    char* dest_bytes = (char*)dest;
    char* src_bytes = (char*)src;
    if (dest_bytes < src_bytes) {
        return memcpy(dest, src, n);
    } else if(dest_bytes > src_bytes) {
        for(size_t i = 0; i < n; i++) {
            dest_bytes[n - i - 1] = src_bytes[n - i - 1];
        }
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
