#include "kernel.h"
#include "thread/waitable.h"
#include "runtime/dotnet/internal_calls.h"
#include "mem/tlsf.h"
#include "debug/term.h"
#include "time/tick.h"

#include <limine.h>

#include <dotnet/gc/heap.h>
#include <dotnet/jit/jit.h>
#include <dotnet/loader.h>
#include <dotnet/gc/gc.h>

#include <thread/scheduler.h>
#include <thread/cpu_local.h>
#include <thread/thread.h>

#include <debug/debug.h>
#include <debug/asan.h>
#include <debug/profiler.h>

#include <util/except.h>
#include <util/string.h>
#include <util/defs.h>

#include <mem/malloc.h>
#include <mem/mem.h>
#include <mem/vmm.h>

#include <time/delay.h>
#include <time/tsc.h>
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
volatile struct limine_framebuffer_request g_limine_framebuffer = { .id = LIMINE_FRAMEBUFFER_REQUEST };

struct limine_framebuffer* g_framebuffers = NULL;
int g_framebuffers_count = 0;

__attribute__((section(".limine_reqs"), used))
static void* m_limine_reqs[] = {
    (void*)&g_limine_bootloader_info,
    (void*)&g_limine_kernel_file,
    (void*)&g_limine_module,
    (void*)&g_limine_smp,
    (void*)&g_limine_memmap,
    (void*)&g_limine_rsdp,
    (void*)&g_limine_kernel_address,
    (void*)&g_limine_framebuffer,
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
    init_tss((void*)info->extra_argument);

    init_vmm_per_cpu();
    CHECK_AND_RETHROW(init_cpu_locals());
    CHECK_AND_RETHROW(init_apic());
    CHECK_AND_RETHROW(init_scheduler_per_core());

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
    sync_tick();

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
 * Load all the assemblies
 */
static struct limine_file* m_assemblies = NULL;

/**
 * Can be used to run self testing, not called by default
 */
static inline void self_test() {
    TRACE("Running self-test");
    waitable_self_test();
    scheduler_self_test();
    semaphore_self_test();
    mutex_self_test();
    TRACE("self-test finished");
}

static void kernel_startup() {
    err_t err = NO_ERROR;

    // wait for all the cores to exit the preboot stuff
    while (atomic_load_explicit(&m_startup_count, memory_order_relaxed) != g_limine_smp.response->cpu_count) {
        __asm__ ("pause");
    }

    // copy the framebuffer information
    if (g_limine_framebuffer.response != NULL) {
        g_framebuffers_count = (int)g_limine_framebuffer.response->framebuffer_count;
        if (g_framebuffers_count > 0) {
            g_framebuffers = palloc(g_framebuffers_count);
            for (int i = 0; i < g_framebuffers_count; i++) {
                g_framebuffers[i] = *g_limine_framebuffer.response->framebuffers[i];
            }
        }
    }

    // reclaim bootloader memory and make sure all the boot information is zeroed
    CHECK_AND_RETHROW(palloc_reclaim());
    g_limine_bootloader_info.response = NULL;
    g_limine_kernel_file.response = NULL;
    g_limine_module.response = NULL;
    g_limine_smp.response = NULL;
    g_limine_memmap.response = NULL;
    g_limine_rsdp.response = NULL;
    g_limine_kernel_address.response = NULL;
    g_limine_framebuffer.response = NULL;

    // uncomment if you want to debug some stuff and
    // make sure that the kernel passes self-tests
//    self_test();

    TRACE("Entered kernel thread!");

    // Initialize the runtime
    CHECK_AND_RETHROW(init_gc());
    CHECK_AND_RETHROW(init_heap());
    CHECK_AND_RETHROW(init_jit());
    CHECK_AND_RETHROW(init_kernel_internal_calls());

    TRACE("Loading all assemblies");
    for (int i = 0; i < arrlen(m_assemblies); i++) {
        struct limine_file* file = &m_assemblies[i];
        TRACE("Loading `%s`", file->path);

        // on the last assembly disable the kernel
        // terminal so we won't have anything weird
        if (i == arrlen(m_assemblies) - 1) {
            TRACE("Disabling framebuffer");
            term_disable();
        }

        if (i == 0) {
            profiler_start();
            // load the corelib
            CHECK_AND_RETHROW(loader_load_corelib(file->address, file->size));
            profiler_stop();

            // we are now going to initialize a managed thread for this thread, so stuff like the ThreadPool
            // will work properly even with this initialization thread
            System_Type threadType = assembly_get_type_by_name(g_corelib, "Thread", "System.Threading");
            CHECK(threadType != NULL);

            // jit the thread type, since we are going to create an instance of it
            CHECK_AND_RETHROW(jit_type(threadType));

            // get a reference to the current thread to put in the managed part, we are going
            // to pass our reference to this class essentially
            void* thread = GC_NEW(threadType);

            // set the thread handle field
            System_Reflection_FieldInfo threadHandle = type_get_field_cstr(threadType, "_threadHandle");
            CHECK(!field_is_static(threadHandle));
            *((thread_t**)(thread + threadHandle->MemoryOffset)) = put_thread(get_current_thread());

            // TODO: set the thread name?

            // now attach it
            get_current_thread()->tcb->managed_thread = thread;
        } else {
            // load it as a normal assembly
            System_Reflection_Assembly assembly = NULL;
            if (IS_ERROR(loader_load_assembly(file->address, file->size, &assembly))) {
                WARN("Failed to load assembly `%s`, ignoring...", file->path);
                continue;
            }

            // if we have an entry point then run it
            if (assembly->EntryPoint != NULL) {
                TRACE("\tRunning entry point");

                // jit it
                CHECK_AND_RETHROW(jit_method(assembly->EntryPoint));

                // run it
                method_result_t(*entry_point)() = assembly->EntryPoint->MirFunc->addr;
                method_result_t result = entry_point();
                if (result.exception != NULL) {
                    WARN("Got exception: \"%U\" (of type `%U`), ignoring",
                         result.exception->Message, OBJECT_TYPE(result.exception)->Name);
                } else {
                    if (assembly->EntryPoint->ReturnType != NULL) {
                        TRACE("\tReturned: %d", result.value);
                    }
                }
            }
        }
    }

cleanup:
    ASSERT(!IS_ERROR(err));
}

/**
 * The TSS used for the BSP
 */
static uint8_t m_bsp_tss[TSS_ALLOC_SIZE];

bool str_ends_with(const char* str, const char* endswith) {
    size_t len = strlen(str), endswith_len = strlen(endswith);
    if (len < endswith_len) return false;
    return memcmp(str + len - endswith_len, endswith, endswith_len) == 0;
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
    TRACE("Hello from TomatOS!");
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
    TRACE("\t%p-%p (%S): Stack pool", STACK_POOL_START, STACK_POOL_END, STACK_POOL_SIZE);
    heap_dump_mapping();
    TRACE("\t%p-%p (%S): Recursive paging", RECURSIVE_PAGING_START, RECURSIVE_PAGING_END, RECURSIVE_PAGING_SIZE);
    TRACE("\t%p-%p (%S): Kernel heap", KERNEL_HEAP_START, KERNEL_HEAP_END, KERNEL_HEAP_SIZE);

    // initialize the whole memory subsystem for the current CPU,
    // will initialize the rest afterwards, init_vmm also initializes
    // the apic (since it needs to be mapped before the switch)
    CHECK_AND_RETHROW(init_vmm());
    CHECK_AND_RETHROW(init_palloc());
    CHECK_AND_RETHROW(init_cpu_locals());
    init_tss(m_bsp_tss);
    vmm_switch_allocator();
#ifdef __KASAN__
    CHECK_AND_RETHROW(init_kasan());
#endif
    CHECK_AND_RETHROW(init_malloc());

    // load symbols for nicer debugging
    // NOTE: must be done after allocators are done
    debug_load_symbols();

    // initialize misc kernel utilities
    CHECK_AND_RETHROW(init_acpi());
    CHECK_AND_RETHROW(init_delay());
    CHECK_AND_RETHROW(init_tsc());

    //
    // Do SMP startup
    //

    // if we have the smp tag then we are SMP, set the cpu count
    m_cpu_count = g_limine_smp.response->cpu_count;

    // initialize per cpu variables
    CHECK_AND_RETHROW(init_scheduler());
    CHECK_AND_RETHROW(init_tls());

    // now actually do smp startup
    TRACE("SMP Startup");
    uint64_t tss_memory = (uint64_t)palloc(TSS_ALLOC_SIZE * g_limine_smp.response->cpu_count);

    for (int i = 0; i < g_limine_smp.response->cpu_count; i++) {

        // right now we assume that the apic id is always less than the cpu count
        CHECK(g_limine_smp.response->cpus[i]->lapic_id < g_limine_smp.response->cpu_count);

        if (g_limine_smp.response->cpus[i]->lapic_id == g_limine_smp.response->bsp_lapic_id) {
            TRACE("\tCPU #%d - BSP", g_limine_smp.response->bsp_lapic_id);
            CHECK(g_limine_smp.response->bsp_lapic_id == get_apic_id());
            continue;
        }

        g_limine_smp.response->cpus[i]->extra_argument = tss_memory + i * TSS_ALLOC_SIZE;

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

        // If it ends with dll, then load it as a dll
        if (str_ends_with(file->path, ".dll")) {
            arrpush(m_assemblies, *file);
            size_t len = strlen(file->path);
            arrlast(m_assemblies).path = malloc(len + 1);
            CHECK(arrlast(m_assemblies).path != NULL);
            memcpy(arrlast(m_assemblies).path, file->path, len);
        } else {
            // TODO: save as a module that the kernel can search for?
            //       maybe queue it as a tar for initramfs?
        }

        TRACE("\t%s - %S", file->path, file->size);
    }

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
    sync_tick();
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
