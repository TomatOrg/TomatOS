#pragma once

#define _SC_PAGE_SIZE 0

static inline long sysconf(int name) {
    if (name == _SC_PAGE_SIZE) {
        return 4096;
    } else {
        return -1;
    }
}

// TODO: return app domain id
static inline int getpid(void) {
    return 0;
}
