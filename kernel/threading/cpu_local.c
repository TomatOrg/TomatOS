#include "cpu_local.h"
#include "arch/intrin.h"
#include "arch/msr.h"

extern char __cpu_local_size[];

static void* CPU_LOCAL m_per_cpu_base;

err_t init_cpu_locals() {
    err_t err = NO_ERROR;

    // allocate and set the gs base
    void* ptr = malloc((size_t) __cpu_local_size);
    CHECK(ptr != NULL);
    __writemsr(MSR_IA32_GS_BASE, (uintptr_t)ptr);

    // set the base in here, for easy access in the future
    m_per_cpu_base = ptr;

cleanup:
    return err;
}

void* get_cpu_local_base(__seg_gs void* ptr) {
    return m_per_cpu_base + (size_t)ptr;
}
