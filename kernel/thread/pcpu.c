#include "pcpu.h"

#include <lib/elf64.h>
#include <limine.h>
#include <arch/intrin.h>
#include <lib/string.h>
#include <mem/alloc.h>

/**
 * we need the elf so we can properly allocate the pcpu segment
 */
extern struct limine_kernel_file_request g_limine_kernel_file_request;

/**
 * The id of the current cpu
 */
static CPU_LOCAL int m_cpu_id;

err_t pcpu_init_per_core(int cpu_id) {
    err_t err = NO_ERROR;

    // get the TLS segment
    void* elf_base = g_limine_kernel_file_request.response->kernel_file->address;
    Elf64_Ehdr* ehdr = elf_base;
    Elf64_Phdr* phdrs = elf_base + ehdr->e_phoff;
    Elf64_Phdr* phdr = NULL;
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type != PT_TLS) {
            continue;
        }
        CHECK(phdr == NULL);
        phdr = phdrs;
    }
    CHECK(phdr != NULL);

    // figure the actual size and alignment
    size_t tls_size = phdr->p_memsz;

    // allocate the tls storage and initialize it
    void* tls = mem_alloc(tls_size + sizeof(uintptr_t));
    CHECK_ERROR(tls != NULL, ERROR_OUT_OF_MEMORY);
    // memcpy(tls, elf_base + phdr->p_offset, phdr->p_filesz);

    // set the linear address
    *(void**)(tls + tls_size) = tls + tls_size;

    // and write it to the fs base
    __wrmsr(MSR_IA32_FS_BASE, (uintptr_t)(tls + tls_size));

    // now write to the cpu id
    m_cpu_id = cpu_id;

cleanup:
    return err;
}

int get_cpu_id() {
    return m_cpu_id;
}
