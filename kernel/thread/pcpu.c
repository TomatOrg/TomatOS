#include "pcpu.h"

#include <lib/elf64.h>
#include <limine.h>
#include <lib/string.h>
#include <mem/phys.h>

#include "limine_requests.h"
#include "arch/apic.h"
#include "arch/intrin.h"
#include "mem/alloc.h"
#include "time/tsc.h"

extern char __start_pcpu_data[];
extern char __stop_pcpu_data[];

/**
 * The id of the current cpu
 */
static CPU_LOCAL int m_cpu_id;

/**
 * The fsbase of the current cpu
 */
static CPU_LOCAL size_t m_cpu_fs_base;

/**
 * All the fs-bases of all the cores
 */
static uintptr_t* m_all_fs_bases;

void init_early_pcpu(void) {
    // the BSP uses offset zero, because the per-cpu variables are
    // allocated inside of the kernel, it means that the BSP can
    // just use that allocation without worrying
    __wrmsr(MSR_IA32_FS_BASE, 0);
    m_cpu_id = 0;
    m_cpu_fs_base = 0;
}

err_t init_pcpu(int cpu_count) {
    err_t err = NO_ERROR;

    // TODO: check if TSC deadline is supported, if so use it, otherwise use
    //       lapic timer or whatever else we want
    m_all_fs_bases = mem_alloc(sizeof(*m_all_fs_bases) * cpu_count);
    CHECK_ERROR(m_all_fs_bases != NULL, ERROR_OUT_OF_MEMORY);
    memset(m_all_fs_bases, 0, sizeof(*m_all_fs_bases) * cpu_count);

    // the BSP is always at offset zero
    m_all_fs_bases[0] = 0;

cleanup:
    return err;
}

err_t pcpu_init_per_core(int cpu_id) {
    err_t err = NO_ERROR;

    char* data = mem_alloc(__stop_pcpu_data - __start_pcpu_data);
    CHECK_ERROR(data != NULL, ERROR_OUT_OF_MEMORY);

    size_t offset =  data - __start_pcpu_data;
    __wrmsr(MSR_IA32_FS_BASE, offset);

    m_all_fs_bases[cpu_id] = offset;
    m_cpu_id = cpu_id;
    m_cpu_fs_base = offset;

cleanup:
    return err;
}

int get_cpu_id() {
    return m_cpu_id;
}

bool pcpu_check_timer(void) {
    return true;
}

void* pcpu_get_pointer(__seg_fs void* ptr) {
    return (void*)(m_cpu_fs_base + (uintptr_t)ptr);
}

void* pcpu_get_pointer_of(__seg_fs void* ptr, int cpu_id) {
    return (void*)(m_all_fs_bases[cpu_id] + (uintptr_t)ptr);
}
