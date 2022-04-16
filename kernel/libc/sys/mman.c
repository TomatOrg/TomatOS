#include "mman.h"

#include <mem/mem.h>

int mprotect(void *addr, size_t len, int prot) {
    return IS_ERROR(vmm_set_perms(addr, ALIGN_UP(len, PAGE_SIZE) / PAGE_SIZE, prot)) ? -1 : 0;
}

void* mmap(void* addr, size_t len, int prot, int flags, int fildes, off_t off) {
    ASSERT(off == 0);
    ASSERT(fildes < 0);
    ASSERT(flags == (MAP_PRIVATE | MAP_ANONYMOUS));
    void* ptr = palloc(ALIGN_UP(len, PAGE_SIZE));
    if (ptr == NULL) {
        return MAP_FAILED;
    }
    mprotect(ptr, len, prot);
    return ptr;
}

int munmap(void* addr, size_t len) {
    if (mprotect(addr, len, PROT_READ | PROT_WRITE) != 0) {
        return -1;
    }
    pfree(addr);
    return 0;
}