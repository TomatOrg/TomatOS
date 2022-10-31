#pragma once

#define _SC_PAGE_SIZE 0
#define _SC_PAGESIZE 0

static inline long sysconf(int name) {
    if (name == _SC_PAGE_SIZE) {
        return 4096;
    } else {
        WARN("Unknown sysconf %d\n", name);
        return -1;
    }
}

// TODO: return app domain id
static inline int getpid(void) {
    return 0;
}

// TODO: mimalloc uses this
#define _PC_PATH_MAX 0
static inline long pathconf(const char *path, int name) {
    return 0;
}
