#include "pcpu.h"

#include <lib/elf64.h>
#include <limine.h>
#include <lib/string.h>
#include <mem/phys.h>

#include "limine_requests.h"
#include "arch/apic.h"
#include "arch/intrin.h"
#include "time/tsc.h"

typedef struct pcpu_timer {
    void (*set_deadline)(uint64_t tsc_deadline);
    void (*clear)(void);
} pcpu_timer_t;

/**
 * The id of the current cpu
 */
static CPU_LOCAL int m_cpu_id;

/**
 * The per-cpu data of all the cores pre-allocated
 */
static uint8_t* m_per_cpu_data;

/**
 * The size of each cpu's data
 */
static size_t m_per_cpu_size;

/**
 * The timer we are using
 */
static pcpu_timer_t m_pcpu_timer = {};

err_t pcpu_init(int cpu_count) {
    err_t err = NO_ERROR;

    // get the TLS segment
    void* elf_base = g_limine_executable_file_request.response->executable_file->address;
    Elf64_Ehdr* ehdr = elf_base;
    Elf64_Phdr* phdrs = elf_base + ehdr->e_phoff;
    Elf64_Phdr* phdr = NULL;
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type != PT_TLS) {
            continue;
        }
        CHECK(phdr == NULL);
        phdr = &phdrs[i];
    }
    CHECK(phdr != NULL);

    // figure the actual size and alignment
    m_per_cpu_size = phdr->p_memsz;
    CHECK(m_per_cpu_size >= phdr->p_filesz);
    m_per_cpu_size += sizeof(uintptr_t);

    // allocate everything
    m_per_cpu_data = early_phys_alloc(m_per_cpu_size * cpu_count);
    CHECK_ERROR(m_per_cpu_data != NULL, ERROR_OUT_OF_MEMORY, "%zu", m_per_cpu_size * cpu_count);

    // initialize it per-cpu
    for (int i = 0; i < cpu_count; i++) {
        // initialize the data to be the default values
        uint8_t* tls_start = m_per_cpu_data + i * m_per_cpu_size;
        memset(tls_start, 0, m_per_cpu_size);
        memcpy(tls_start, elf_base + phdr->p_offset, phdr->p_filesz);

        // write the tls pointer right at the end
        void* tcb = tls_start + (m_per_cpu_size - sizeof(uintptr_t));
        *(void**)(tcb) = tcb;
    }

    // TODO: check if TSC deadline is supported, if so use it, otherwise use
    //       lapic timer or whatever else we want

    if (tsc_deadline_is_supported()) {
        TRACE("timer: using TSC deadline");
        m_pcpu_timer.set_deadline = tsc_timer_set_deadline;
        m_pcpu_timer.clear = tsc_timer_clear;
    } else {
        TRACE("timer: using APIC timer");
        m_pcpu_timer.set_deadline = lapic_timer_set_deadline;
        m_pcpu_timer.clear = lapic_timer_clear;
    }

cleanup:
    return err;
}

void pcpu_init_per_core(int cpu_id) {
    // write the fs-base of the current cpu
    void* tls_start = m_per_cpu_data + (cpu_id * m_per_cpu_size);
    void* tcb = tls_start + (m_per_cpu_size - sizeof(uintptr_t));
    __wrmsr(MSR_IA32_FS_BASE, (uintptr_t)tcb);

    // now write to the cpu id
    m_cpu_id = cpu_id;
}

int get_cpu_id() {
    return m_cpu_id;
}

bool pcpu_check_timer(void) {
    return true;
}

void pcpu_timer_set_deadline(uint64_t tsc_deadline) {
    m_pcpu_timer.set_deadline(tsc_deadline);
}

void pcpu_timer_clear(void) {
    m_pcpu_timer.clear();
}
