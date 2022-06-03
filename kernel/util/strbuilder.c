#include "strbuilder.h"

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

void strbuilder_uint(strbuilder_t* builder, size_t n) {
    size_t start = arrlenu(builder->buf), target_len = 20; // maximum number of digits in a 64bit number
    arrsetcap(builder->buf, arrlen(builder->buf) + target_len);
    int len = snprintf(builder->buf + start, 20, "%lu", n); // TODO: replace this with a handrolled atoi
    arrsetlen(builder->buf, arrlen(builder->buf) + len);
}

char* strbuilder_get(strbuilder_t* builder) {
    size_t start = arrlenu(builder->buf);
    if (start == 0 || builder->buf[start-1] != '\0') {
        arrput(builder->buf, '\0');
    }
    return builder->buf;
} 