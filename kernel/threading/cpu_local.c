#include "cpu_local.h"
#include "arch/intrin.h"
#include "arch/msr.h"
#include "kernel.h"

extern char __cpu_local_size[];

static void* CPU_LOCAL m_per_cpu_base;

static int CPU_LOCAL m_cpu_id;

/**
 * All the cpus
 */
static void** m_per_cpu_base_list;

err_t init_cpu_locals() {
    err_t err = NO_ERROR;

    // allocate and set the gs base
    void* ptr = malloc((size_t) __cpu_local_size);
    CHECK(ptr != NULL);
    __writemsr(MSR_IA32_GS_BASE, (uintptr_t)ptr);

    // set the base in here, for easy access in the future
    m_per_cpu_base = ptr;

    // setup the list of per cpu bases
    if (m_per_cpu_base_list == NULL) {
        m_per_cpu_base_list = malloc(sizeof(void*) * get_cpu_count());
    }

    // set the current cpu pointer in the array
    m_cpu_id = get_apic_id();
    m_per_cpu_base_list[get_cpu_id()] = ptr;

cleanup:
    return err;
}

void* get_cpu_local_base(__seg_gs void* ptr) {
    return m_per_cpu_base + (size_t)ptr;
}

void* get_cpu_base(int cpu, __seg_gs void* ptr) {
    return m_per_cpu_base_list[cpu] + (size_t)ptr;
}

int get_cpu_id() {
    return m_cpu_id;
}
