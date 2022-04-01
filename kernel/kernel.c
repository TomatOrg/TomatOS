#include "kernel.h"

#include "stivale2.h"
#include "runtime/dotnet/loader.h"

#include <runtime/dotnet/gc/gc.h>

#include <threading/scheduler.h>
#include <threading/cpu_local.h>
#include <threading/thread.h>

#include <debug/debug.h>

#include <util/except.h>
#include <util/string.h>
#include <util/defs.h>

#include <mem/malloc.h>
#include <mem/mem.h>
#include <mem/vmm.h>

#include <time/delay.h>
#include <time/timer.h>
#include <acpi/acpi.h>

#include <arch/intrin.h>
#include <arch/apic.h>
#include <arch/regs.h>
#include <arch/gdt.h>
#include <arch/idt.h>

#include <stdatomic.h>
#include <stdalign.h>
#include <stddef.h>

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

struct stivale2_module* get_stivale2_module(const char* name) {
    static struct stivale2_struct_tag_modules* modules = NULL;
    if (modules == NULL) {
        modules = get_stivale2_tag(STIVALE2_STRUCT_TAG_MODULES_ID);
    }

    if (modules != NULL) {
        // get the corlib first
        for (int i = 0; i < modules->module_count; i++) {
            struct stivale2_module* module = &modules->modules[i];
            if (strcmp(module->string, name) == 0) {
                return module;
            }
        }
    }

    return NULL;
}

void* get_kernel_file() {
    static void* kernel = NULL;
    if (kernel == NULL) {
        struct stivale2_struct_tag_kernel_file* f = get_stivale2_tag(STIVALE2_STRUCT_TAG_KERNEL_FILE_ID);
        if (f == NULL) {
            struct stivale2_struct_tag_kernel_file_v2* f2 = get_stivale2_tag(STIVALE2_STRUCT_TAG_KERNEL_FILE_V2_ID);
            if (f2 != NULL) {
                kernel = (void*)f2->kernel_file;
            }
        } else {
            kernel = (void*)f->kernel_file;
        }
    }
    return kernel;
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
static _Atomic(int) m_startup_count = 0;

/**
 * Set to true if we got an error on smp setup
 */
static _Atomic(bool) m_smp_error = false;

/**
 * Start the scheduler
 */
static _Atomic(bool) m_start_scheduler = false;

static _Atomic(size_t) m_cpu_count = 1;

size_t get_cpu_count() {
    return m_cpu_count;
}

static void per_cpu_start(struct stivale2_smp_info* info) {
    err_t err = NO_ERROR;

    // just like the bsp setup all the cpu stuff
    enable_cpu_features();
    init_gdt();
    init_idt();
    init_vmm_per_cpu();
    CHECK_AND_RETHROW(init_apic());
    CHECK_AND_RETHROW(init_cpu_locals());

    // move to higher half
    info = PHYS_TO_DIRECT(info);

    // make sure this is valid
    CHECK(info->lapic_id == get_apic_id());

    // we are ready!
    TRACE("\tCPU #%d", info->lapic_id);

cleanup:
    if (IS_ERROR(err)) {
        // set that we got an error
        atomic_store(&m_smp_error, true);
        TRACE("\tError on CPU #%d");
    }

    // we done with this CPU
    atomic_fetch_add(&m_startup_count, 1);

    // wait until the kernel wakes us
    // up to start scheduling
    while (!atomic_load_explicit(&m_start_scheduler, memory_order_relaxed)) {
        __builtin_ia32_pause();
    }

    // start scheduling!
    TRACE("\tCPU #%d", info->lapic_id);
    scheduler_startup();

    // we should not have reached here
    ERROR("Should not have reached here?!");
    _disable();
    while (1) asm("hlt");
}

/**
 * This is the module of the corelib, will be freed once the corelib is loaded
 */
static void* m_corelib_module;
static size_t m_corelib_module_size;

static void start_thread() {
    err_t err = NO_ERROR;

    TRACE("Entered kernel thread!");

    // Initialize the runtime
    CHECK_AND_RETHROW(init_gc());
    CHECK_AND_RETHROW(loader_load_corelib(m_corelib_module, m_corelib_module_size));

cleanup:
    ASSERT(!IS_ERROR(err));
    TRACE("Bai Bai!");
    while(1);
}

void _start(struct stivale2_struct* stivale2) {
    err_t err = NO_ERROR;

    // we need to setup SSE support as soon as possible because
    // we don't compile without support for it
    enable_cpu_features();

    // now setup idt/gdt
    init_gdt();
    init_idt();
    early_init_apic();

    // Quickly setup everything so we can
    // start debugging already
    m_stivale2 = stivale2;
    trace_init();
    TRACE("Hello from pentagon!");
    TRACE("\tBootloader: %s (%s)", stivale2->bootloader_brand, stivale2->bootloader_version);

    TRACE("Kernel address map:");
    TRACE("\t%p-%p (%S): Kernel direct map", DIRECT_MAP_START, DIRECT_MAP_END, DIRECT_MAP_SIZE);
    TRACE("\t%p-%p (%S): Buddy Tree", BUDDY_TREE_START, BUDDY_TREE_END, BUDDY_TREE_SIZE);
    TRACE("\t%p-%p (%S): Recursive paging", RECURSIVE_PAGING_START, RECURSIVE_PAGING_END, RECURSIVE_PAGING_SIZE);
    TRACE("\t%p-%p (%S): Stack pool", STACK_POOL_START, STACK_POOL_END, STACK_POOL_SIZE);
    TRACE("\t%p-%p (%S): Kernel heap", KERNEL_HEAP_START, KERNEL_HEAP_END, KERNEL_HEAP_SIZE);

    // initialize the whole memory subsystem for the current CPU,
    // will initialize the rest afterwards, init_vmm also initializes
    // the apic (since it needs to be mapped before the switch)
    CHECK_AND_RETHROW(init_vmm());
    CHECK_AND_RETHROW(init_palloc());
    vmm_switch_allocator();
    CHECK_AND_RETHROW(init_malloc());

    // load symbols for nicer debugging
    // NOTE: must be done after allocators are done
    void* kernel = get_kernel_file();
    CHECK(kernel != NULL);
    debug_load_symbols(kernel);

    // initialize misc kernel utilities
    CHECK_AND_RETHROW(init_acpi());
    CHECK_AND_RETHROW(init_delay());
    CHECK_AND_RETHROW(init_timer());

    //
    // Do SMP startup
    //

    // get the smp tag
    struct stivale2_struct_tag_smp* smp = get_stivale2_tag(STIVALE2_STRUCT_TAG_SMP_ID);

    // if we have the smp tag then we are SMP, set the cpu count
    if (smp != NULL) {
        m_cpu_count = smp->cpu_count;
    }

    // initialize per cpu variables
    CHECK_AND_RETHROW(init_cpu_locals());
    CHECK_AND_RETHROW(init_scheduler());
    CHECK_AND_RETHROW(init_tls(kernel));

    // now actually do smp startup
    if (smp != NULL) {
        TRACE("SMP Startup");
        for (int i = 0; i < smp->cpu_count; i++) {

            // right now we assume that the apic id is always less than the cpu count
            CHECK(smp->smp_info[i].lapic_id < smp->cpu_count);

            if (smp->smp_info[i].lapic_id == smp->bsp_lapic_id) {
                TRACE("\tCPU #%d - BSP", smp->bsp_lapic_id);
                CHECK(smp->bsp_lapic_id == get_apic_id());
                continue;
            }

            // setup the entry for the given CPU
            smp->smp_info[i].target_stack = (uintptr_t)(palloc(PAGE_SIZE) + PAGE_SIZE);
            smp->smp_info[i].goto_address = (uintptr_t)per_cpu_start;
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
        CHECK(get_apic_id() == 0);
    }

    // load the corelib module
    struct stivale2_module* module = get_stivale2_module("Corelib.dll");
    CHECK_ERROR(module != NULL, ERROR_NOT_FOUND);
    m_corelib_module_size = module->end - module->begin;
    m_corelib_module = malloc(m_corelib_module_size);
    memcpy(m_corelib_module, (void*)module->begin, m_corelib_module_size);
    TRACE("Corelib: %S", m_corelib_module_size);

    // reclaim memory
    CHECK_AND_RETHROW(palloc_reclaim());

    TRACE("Kernel init done");

    // must have preemption, this is fine because we don't actually
    // have the timer setup yet, so it will not actually preempt
    __writecr8(PRIORITY_NORMAL);

    // create the kernel start thread
    thread_t* thread = create_thread(start_thread, NULL, "kernel/start_thread");
    CHECK(thread != NULL);
    scheduler_ready_thread(thread);

    TRACE("Starting up the scheduler");
    atomic_store_explicit(&m_start_scheduler, true, memory_order_release);
    TRACE("\tCPU #%d - BSP", get_apic_id());
    scheduler_startup();

cleanup:
    if (IS_ERROR(err)) {
        ERROR("Error in kernel initializing!");
    } else {
        ERROR("Should not have reached here?!");
    }

    _disable();
    while (1) asm("hlt");
}
