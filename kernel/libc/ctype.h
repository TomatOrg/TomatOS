#pragma once

static inline int islower(int c) {
    return ((unsigned)c - 'a') < 26;
}

static inline int toupper(int c) {
    if (islower(c)) return c & 0x5f;
    return c;
}
