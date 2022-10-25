#pragma once

#include <stddef.h>

// according to vmm.h
#define PROT_NONE       (1 << 3)
#define PROT_READ       0
#define PROT_WRITE      (1 << 0)
#define PROT_EXEC       (1 << 1)

// dummies, we don't actually care
#define MAP_ANONYMOUS   (1 << 0)
#define MAP_PRIVATE     (1 << 1)

// we are only going to return direct map stuff
// so null is a good value
#define MAP_FAILED NULL

int mprotect(void* addr, size_t len, int prot);

typedef size_t off_t;

void* mmap(void* addr, size_t len, int prot, int flags, int fildes, off_t off);

int munmap(void* addr, size_t len);

#define MADV_DONTNEED 0
static inline int madvise(void *addr, size_t length, int advice) { return 0; }