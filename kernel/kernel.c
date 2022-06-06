#include "kernel.h"

#include <limine.h>

#include <runtime/dotnet/heap.h>
#include <dotnet/jit/jit.h>
#include <dotnet/loader.h>
#include <dotnet/gc/gc.h>

#include <thread/scheduler.h>
#include <thread/cpu_local.h>
#include <thread/thread.h>

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Limine Requests
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

volatile struct limine_bootloader_info_request g_limine_bootloader_info = { .id = LIMINE_BOOTLOADER_INFO_REQUEST };
volatile struct limine_kernel_file_request g_limine_kernel_file = { .id = LIMINE_KERNEL_FILE_REQUEST };
volatile struct limine_module_request g_limine_module = { .id = LIMINE_MODULE_REQUEST };
volatile struct limine_smp_request g_limine_smp = { .id = LIMINE_SMP_REQUEST };
volatile struct limine_memmap_request g_limine_memmap = { .id = LIMINE_MEMMAP_REQUEST };
volatile struct limine_rsdp_request g_limine_rsdp = { .id = LIMINE_RSDP_REQUEST };
volatile struct limine_kernel_address_request g_limine_kernel_address = { .id = LIMINE_KERNEL_ADDRESS_REQUEST };

__attribute__((section(".limine_reqs"), used))
static void* m_limine_reqs[] = {
    (void*)&g_limine_bootloader_info,
    (void*)&g_limine_kernel_file,
    (void*)&g_limine_module,
    (void*)&g_limine_smp,
    (void*)&g_limine_memmap,
    (void*)&g_limine_rsdp,
    (void*)&g_limine_kernel_address,
    NULL,
};

static err_t validate_limine_modules() {
    err_t err = NO_ERROR;

    // we need all of these
    CHECK(g_limine_kernel_file.response != NULL);
    CHECK(g_limine_module.response != NULL);
    CHECK(g_limine_smp.response != NULL);
    CHECK(g_limine_memmap.response != NULL);
    CHECK(g_limine_rsdp.response != NULL);
    CHECK(g_limine_kernel_address.response != NULL);

cleanup:
    return err;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CPU setup
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
    cr0.EM = 0;
    cr0.MP = 1;
    __writecr0(cr0.packed);

    cr4_t cr4 = { .packed = __readcr4() };
    cr4.OSFXSR = 1;
    cr4.OSXMMEXCPT = 1;
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

static _Atomic(int) m_cpu_count = 1;

int get_cpu_count() {
    return m_cpu_count;
}

static void per_cpu_start(struct limine_smp_info* info) {
    err_t err = NO_ERROR;

    // just like the bsp setup all the cpu stuff
    enable_cpu_features();
    init_gdt();
    init_idt();
    init_vmm_per_cpu();
    CHECK_AND_RETHROW(init_tss());
    CHECK_AND_RETHROW(init_apic());
    CHECK_AND_RETHROW(init_cpu_locals());

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
    atomic_fetch_add(&m_startup_count, 1);
    scheduler_startup();

    // we should not have reached here
    ERROR("Should not have reached here?!");
    _disable();
    while (1)
        __halt();
}

/**
 * The corelib file
 */
static struct limine_file m_corelib_file;

/**
 * The kernel file
 */
static struct limine_file m_kernel_file;

// TODO: driver files

static void kernel_startup() {
    err_t err = NO_ERROR;

    // wait for all the cores to exit the preboot stuff
    while (atomic_load_explicit(&m_startup_count, memory_order_relaxed) != g_limine_smp.response->cpu_count) {
        __asm__ ("pause");
    }

    // reclaim bootloader memory
    CHECK_AND_RETHROW(palloc_reclaim());

    TRACE("Entered kernel thread!");

    // Initialize the runtime
    CHECK_AND_RETHROW(init_gc());
    CHECK_AND_RETHROW(init_heap());
    CHECK_AND_RETHROW(init_jit());

    // load the corelib
    CHECK_AND_RETHROW(loader_load_corelib(m_corelib_file.address, m_corelib_file.size));

    // load the kernel assembly
    System_Reflection_Assembly kernel_asm = NULL;
    CHECK_AND_RETHROW(loader_load_assembly(m_kernel_file.address, m_kernel_file.size, &kernel_asm));

    // call it
    method_result_t(*entry_point)() = kernel_asm->EntryPoint->MirFunc->addr;
    method_result_t result = entry_point();
    CHECK(result.exception == NULL, "Got exception: \"%U\" (of type `%U`)", result.exception->Message, result.exception->vtable->type->Name);
    TRACE("Kernel output: %d", result.value);

cleanup:
    ASSERT(!IS_ERROR(err));
    TRACE("Kernel initialization finished!");
}

void _start(void) {
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
    trace_init();
    TRACE("Hello from pentagon!");
    if (g_limine_bootloader_info.response != NULL) {
        TRACE("\tBootloader: %s (%s)",
              g_limine_bootloader_info.response->name,
              g_limine_bootloader_info.response->version);
    }

    // check the bootloader behaved as expected
    CHECK_AND_RETHROW(validate_limine_modules());

    // for debugging
    TRACE("Kernel address map:");
    TRACE("\t%p-%p (%S): Kernel direct map", DIRECT_MAP_START, DIRECT_MAP_END, DIRECT_MAP_SIZE);
    TRACE("\t%p-%p (%S): Buddy Tree", BUDDY_TREE_START, BUDDY_TREE_END, BUDDY_TREE_SIZE);
    heap_dump_mapping();
    TRACE("\t%p-%p (%S): Recursive paging", RECURSIVE_PAGING_START, RECURSIVE_PAGING_END, RECURSIVE_PAGING_SIZE);
    TRACE("\t%p-%p (%S): Stack pool", STACK_POOL_START, STACK_POOL_END, STACK_POOL_SIZE);
    TRACE("\t%p-%p (%S): Kernel heap", KERNEL_HEAP_START, KERNEL_HEAP_END, KERNEL_HEAP_SIZE);

    // initialize the whole memory subsystem for the current CPU,
    // will initialize the rest afterwards, init_vmm also initializes
    // the apic (since it needs to be mapped before the switch)
    CHECK_AND_RETHROW(init_vmm());
    CHECK_AND_RETHROW(init_palloc());
    CHECK_AND_RETHROW(init_tss());
    vmm_switch_allocator();
    CHECK_AND_RETHROW(init_malloc());

    // load symbols for nicer debugging
    // NOTE: must be done after allocators are done
    debug_load_symbols();

    // initialize misc kernel utilities
    CHECK_AND_RETHROW(init_acpi());
    CHECK_AND_RETHROW(init_delay());
    CHECK_AND_RETHROW(init_timer());

    //
    // Do SMP startup
    //

    // if we have the smp tag then we are SMP, set the cpu count
    m_cpu_count = g_limine_smp.response->cpu_count;

    // initialize per cpu variables
    CHECK_AND_RETHROW(init_cpu_locals());
    CHECK_AND_RETHROW(init_scheduler());
    CHECK_AND_RETHROW(init_tls());

    // now actually do smp startup
    TRACE("SMP Startup");
    for (int i = 0; i < g_limine_smp.response->cpu_count; i++) {

        // right now we assume that the apic id is always less than the cpu count
        CHECK(g_limine_smp.response->cpus[i]->lapic_id < g_limine_smp.response->cpu_count);

        if (g_limine_smp.response->cpus[i]->lapic_id == g_limine_smp.response->bsp_lapic_id) {
            TRACE("\tCPU #%d - BSP", g_limine_smp.response->bsp_lapic_id);
            CHECK(g_limine_smp.response->bsp_lapic_id == get_apic_id());
            continue;
        }

        // go to it
        g_limine_smp.response->cpus[i]->goto_address = per_cpu_start;
    }

    // wait for all the cores to start
    while (atomic_load_explicit(&m_startup_count, memory_order_relaxed) != g_limine_smp.response->cpu_count - 1) {
        __asm__ ("pause");
    }

    // make sure we got no SMP errors
    CHECK(!atomic_load_explicit(&m_smp_error, memory_order_relaxed));

    TRACE("Done CPU startup");

    // load the corelib module
    TRACE("Boot modules:");
    for (int i = 0; i < g_limine_module.response->module_count; i++) {
        struct limine_file* file = g_limine_module.response->modules[i];
        if (strcmp(file->path, "/boot/Corelib.dll") == 0) {
            m_corelib_file = *file;
        } else if (strcmp(file->path, "/boot/Pentagon.dll") == 0) {
            m_kernel_file = *file;
        } else {
            // TODO: if in /drivers/ folder then load it
            // TODO: load a driver manifest for load order
        }
        TRACE("\t%s", file->path);
    }

    TRACE("Corelib: %S", m_corelib_file.size);
    TRACE("Kernel: %S", m_kernel_file.size);

    TRACE("Kernel init done");

    // create the kernel start thread
    thread_t* thread = create_thread(kernel_startup, NULL, "kernel/startup");
    CHECK(thread != NULL);
    scheduler_ready_thread(thread);

    // we are going to use this counter again to know when all
    // cpus exited the bootlaoder supplied stack
    m_startup_count = 0;

    TRACE("Starting up the scheduler");
    atomic_store_explicit(&m_start_scheduler, true, memory_order_release);
    TRACE("\tCPU #%d - BSP", get_apic_id());

    atomic_fetch_add(&m_startup_count, 1);
    scheduler_startup();

cleanup:
    if (IS_ERROR(err)) {
        ERROR("Error in kernel initializing!");
    } else {
        ERROR("Should not have reached here?!");
    }

    _disable();
    while (1)
        __halt();
}
