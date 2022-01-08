#include "kernel.h"

#include "stivale2.h"

#include <util/except.h>
#include <util/string.h>
#include <util/defs.h>

#include <arch/intrin.h>
#include <arch/regs.h>
#include <arch/gdt.h>
#include <arch/idt.h>

#include <stddef.h>

#include <stdalign.h>
#include <mem/mem.h>
#include <dotnet/gc/gc.h>
#include <mem/vmm.h>
#include <sync/ticketlock.h>
#include <stdatomic.h>
#include <dotnet/jit/jitter_internal.h>
#include <mem/stack.h>
#include <mem/malloc.h>

alignas(16)
static char m_entry_stack[SIZE_2MB] = {0};

static struct stivale2_header_tag_framebuffer m_stivale2_framebuffer = {
    .tag = {
        .identifier = STIVALE2_HEADER_TAG_FRAMEBUFFER_ID,
        .next = 0
    },
    .framebuffer_width = 0,
    .framebuffer_height = 0,
    .framebuffer_bpp = 32,
};

static struct stivale2_header_tag_smp m_stivale2_smp = {
    .tag = {
        .identifier = STIVALE2_HEADER_TAG_SMP_ID,
        .next = (uint64_t) &m_stivale2_framebuffer
    },
    .flags = 0,
};

__attribute__((section(".stivale2hdr"), used))
struct stivale2_header g_stivale2_header = {
    .entry_point = 0,
    .stack = (uint64_t) (m_entry_stack + sizeof(m_entry_stack)),
    .flags =    BIT1 |  // request stivale2 struct to have higher half pointers
                BIT2 |  // request PMRs
                BIT3 |  // request fully virtual kernel mappings
                BIT4,   // Should also be enabled according to spec
    .tags = (uint64_t) &m_stivale2_smp
};

static struct stivale2_struct* m_stivale2 = NULL;

void* get_stivale2_tag(uint64_t tag_id) {
    for (
        struct stivale2_tag* tag = (struct stivale2_tag *) m_stivale2->tags;
        tag != NULL;
        tag = (struct stivale2_tag *) tag->next
    ) {
        if (tag->identifier == tag_id) {
            return tag;
        }
    }
    return NULL;
}

/**
 * Enable CPU features we need
 * - Write Protection
 * - FXSAVE
 *
 * TODO: support for XSAVE
 */
static void enable_cpu_features() {
    cr0_t cr0 = { .packed = __readcr0() };
    cr0.WP = 1;
    __writecr0(cr0.packed);

    cr4_t cr4 = { .packed = __readcr4() };
    cr4.OSFXSR = 1;
    __writecr4(cr4.packed);
}

/**
 * How many cpus have started
 */
static int m_startup_count = 0;

/**
 * Set to true if we got an error on smp setup
 */
static bool m_smp_error = false;

static void per_cpu_start(struct stivale2_smp_info* info) {
    err_t err = NO_ERROR;

    // just like the bsp setup all the cpu stuff
    enable_cpu_features();
    init_gdt();
    init_idt();
    init_vmm_per_cpu();

    // move to higher half
    info = PHYS_TO_DIRECT(info);

    // we are ready!
    TRACE("\tCPU #%d", info->lapic_id);

cleanup:
    if (IS_ERROR(err)) {
        // set that we got an error
        atomic_store(&m_smp_error, true);
    }

    // we done with this CPU
    atomic_fetch_add(&m_startup_count, 1);

    // wait until the kernel wakes us
    // up to start scheduling
    while (1) asm ("hlt");
}

void _start(struct stivale2_struct* stivale2) {
    err_t err = NO_ERROR;

    // we need to setup SSE support as soon as possible because
    // we don't compile without support for it
    enable_cpu_features();

    // now setup idt/gdt
    init_gdt();
    init_idt();

    // Quickly setup everything so we can
    // start debugging already
    m_stivale2 = stivale2;
    trace_init();
    TRACE("Hello from pentagon!");

    // initialize the whole memory subsystem for the current CPU,
    // will initialize the rest afterwards
    CHECK_AND_RETHROW(init_vmm());
    CHECK_AND_RETHROW(init_palloc());
    vmm_switch_allocator();
    CHECK_AND_RETHROW(init_malloc());

    struct stivale2_struct_tag_smp* smp = get_stivale2_tag(STIVALE2_STRUCT_TAG_SMP_ID);
    if (smp != NULL) {
        TRACE("SMP Startup");
        for (int i = 0; i < smp->cpu_count; i++) {
            if (smp->smp_info[i].lapic_id == smp->bsp_lapic_id) {
                TRACE("\tCPU #%d - BSP", smp->bsp_lapic_id);
                continue;
            }

            // setup the entry for the given CPU
            smp->smp_info[i].target_stack = (uintptr_t)(palloc(PAGE_SIZE) + PAGE_SIZE);
            atomic_store(&smp->smp_info[i].goto_address, (uintptr_t)per_cpu_start);
        }

        // wait for all the cores to start
        while (atomic_load_explicit(&m_startup_count, memory_order_relaxed) != smp->cpu_count - 1) {
            asm ("pause");
        }

        // make sure we got no SMP errors
        CHECK(!atomic_load_explicit(&m_smp_error, memory_order_relaxed));

        TRACE("Done CPU startup");
    } else {
        TRACE("Bootloader doesn't support SMP startup!");
    }

    // now that we are done with the bootloader we
    // can reclaim all memory
    CHECK_AND_RETHROW(palloc_reclaim());

cleanup:

    if (IS_ERROR(err)) {
        ERROR("Error in kernel initializing!");
    }

    TRACE("Kernel init done");
    while(1) asm ("hlt");
}
