#include "strbuilder.h"
#include <stdint.h>

strbuilder_t strbuilder_new() {
    return (strbuilder_t) { .buf = NULL }; // this is correct, stbds auto-allocates if it's null
}

void strbuilder_free(strbuilder_t* builder) {
    arrfree(builder->buf);
    builder->buf = NULL;
}

void strbuilder_utf16(strbuilder_t* builder, const __CHAR16_TYPE__* str, size_t length) {
    // TODO: actually do UTF16
    size_t start = arrlenu(builder->buf);
    arraddn(builder->buf, length);
    for (size_t i = 0; i < length; i++) {
        (builder->buf)[start+i] = str[i];
    }
}

void strbuilder_cstr(strbuilder_t* builder, const char* str) {
    size_t start = arrlenu(builder->buf), target_len = strlen(str);
    arraddn(builder->buf, target_len);
    memcpy(builder->buf + start, str, target_len);
}

void strbuilder_char(strbuilder_t* builder, char c) {
    arrput(builder->buf, c);
}

int int_log2(uint32_t x) { return 31 - __builtin_clz(x|1); }
size_t num_digits(uint32_t x) {
    static uint32_t table[] = {9, 99, 999, 9999, 99999, 999999, 9999999, 99999999, 999999999};
    int y = (9 * int_log2(x)) >> 5;
    y += x > table[y];
    return y + 1;
}

void strbuilder_uint(strbuilder_t* builder, size_t n) {
    size_t start = arrlenu(builder->buf), target_len = num_digits(n);
            arraddn(builder->buf, target_len);
    for (size_t i = 0; i < target_len; i++) {
        builder->buf[start+target_len-1-i] = '0' + (n % 10);
        n /= 10;
    }
}

char* strbuilder_get(strbuilder_t* builder) {
    size_t start = arrlenu(builder->buf);
    if (start == 0 || builder->buf[start-1] != '\0') {
        arrput(builder->buf, '\0');
    }
    return builder->buf;
}
