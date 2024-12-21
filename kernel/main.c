#include "limine.h"
#include "debug/log.h"
#include "arch/gdt.h"
#include "arch/idt.h"
#include "lib/except.h"
#include "acpi/acpi.h"
#include "arch/intrin.h"
#include "arch/smp.h"
#include "sync/spinlock.h"
#include "mem/phys.h"
#include "mem/virt.h"
#include "arch/regs.h"
#include "mem/alloc.h"

#include <stddef.h>
#include <stdatomic.h>
#include <arch/apic.h>
#include <debug/debug.h>
#include <lib/string.h>
#include <mem/gc/gc.h>
#include <sync/mutex.h>
#include <thread/pcpu.h>
#include <thread/scheduler.h>
#include <time/tsc.h>

#include <tomatodotnet/tdn.h>
#include <tomatodotnet/jit/jit.h>

/**
 * Use limine base revision 1 since its the newest
 */
LIMINE_BASE_REVISION(1);

/**
 * For logging, on other reason
 */
LIMINE_REQUEST struct limine_bootloader_info_request g_limine_bootloader_info_request = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST,
};

LIMINE_REQUEST struct limine_smp_request g_limine_smp_request = {
    .id = LIMINE_SMP_REQUEST
};

__attribute__((used, section(".limine_requests_delimiter")))
static volatile LIMINE_REQUESTS_DELIMITER;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// First thread
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * The init thread
 */
static thread_t* m_init_thread;

LIMINE_REQUEST struct limine_module_request g_limine_modules_request = {
    .id = LIMINE_MODULE_REQUEST
};

static struct limine_file* get_module_by_name(const char* name) {
    struct limine_module_response* response = g_limine_modules_request.response;
    if (response == NULL) {
        return NULL;
    }

    for (int i = 0; i < response->module_count; i++) {
        struct limine_file* module = response->modules[i];
        if (strcmp(module->path, name) == 0) {
            return module;
        }
    }

    return NULL;
}

static void init_thread_entry(void* arg) {
     err_t err = NO_ERROR;

     LOG_INFO("Init thread started");

     // initialize the garbage collector
     gc_init();

     // first load the corelib
     struct limine_file* corelib = get_module_by_name("/System.Private.CoreLib.dll");
     CHECK(corelib != NULL, "Failed to find corelib");
     TDN_RETHROW(tdn_load_assembly_from_memory(corelib->address, corelib->size, NULL));

    // load the kernel itself
    // struct limine_file* kernel = get_module_by_name("/Tomato.Kernel.dll");
    // CHECK(kernel != NULL, "Failed to find kernel");
    // RuntimeAssembly kernel_assembly;
    // TDN_RETHROW(tdn_load_assembly_from_memory(kernel->address, kernel->size, &kernel_assembly));

    // jit the entry point and call it
    // TDN_RETHROW(tdn_jit_method(kernel_assembly->EntryPoint));
    // void (*entry_point)(void) = kernel_assembly->EntryPoint->MethodPtr;
    // entry_point();

cleanup:
     if (IS_ERROR(err)) {
         LOG_CRITICAL("Can't continue loading the OS");
     }
     (void)err;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Early startup
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * For waiting until all cpus are finished initializing
 */
static atomic_size_t m_smp_count = 0;

/**
 * If we get any failure then we will mark it
 */
static atomic_bool m_smp_fail = false;

static void set_cpu_features() {
    // PG/PE - required for long mode
    // MP - required for SSE
    __writecr0(CR0_PG | CR0_PE | CR0_MP);

    // PAE - required for long mode
    // OSFXSR/OSXMMEXCPT - required for SSE
    __writecr4(CR4_PAE | CR4_OSFXSR | CR4_OSXSAVE | CR4_OSXMMEXCPT);

    // TODO: disable split locks

    // TODO: enable all speculation that we want

    // TODO: enable all the extensions that we want to support

    init_lapic_per_core();
}

static void halt() {
    asm("cli");
    for (;;) {
        asm("hlt");
    }
}

static void smp_entry(struct limine_smp_info* info) {
    err_t err = NO_ERROR;

    LOG_DEBUG("smp: \tCPU#%d - LAPIC#%d", info->extra_argument, info->lapic_id);

    //
    // Start by setting the proper CPU context
    //
    init_gdt();
    init_idt();
    set_cpu_features();
    switch_page_table();

    //
    // And now setup the per-cpu
    //
    pcpu_init_per_core(info->extra_argument);
    init_phys_per_cpu();
    RETHROW(init_tss());

    // and now we can init
    RETHROW(scheduler_init_per_core());

    // we are done
    m_smp_count++;

    // we can trigger the scheduler,
    scheduler_start_per_core();

cleanup:
    // if we got an error mark it
    if (IS_ERROR(err)) {
        m_smp_fail = true;
        m_smp_count++;
    }
    halt();
}

void _start() {
    err_t err = NO_ERROR;

    // make early logging work
    init_early_logging();

    // Welcome!
    LOG("------------------------------------------------------------------------------------------------------------");
    LOG("TomatOS");
    LOG("------------------------------------------------------------------------------------------------------------");

    // basic boot information
    if (g_limine_bootloader_info_request.response != NULL) {
        LOG_DEBUG("Bootloader: %s - %s",
            g_limine_bootloader_info_request.response->name,
            g_limine_bootloader_info_request.response->version);
    }

    // check the available string features
    string_verify_features();

    //
    // early cpu init, this will take care of having interrupts
    // and a valid GDT already
    //
    init_gdt();
    init_idt();

    //
    // setup the basic memory management
    //
    RETHROW(init_virt_early());
    RETHROW(init_phys());

    //
    // setup the per-cpu data of the current cpu
    //
    if (g_limine_smp_request.response != NULL) {
        g_cpu_count = g_limine_smp_request.response->cpu_count;

        // allocate all the storage needed
        RETHROW(pcpu_init(g_limine_smp_request.response->cpu_count));

        // now find the correct cpu id and set it
        bool found = false;
        for (int i = 0; i < g_limine_smp_request.response->cpu_count; i++) {
            if (g_limine_smp_request.response->cpus[i]->lapic_id == g_limine_smp_request.response->bsp_lapic_id) {
                pcpu_init_per_core(i);
                found = true;
                break;
            }
        }
        CHECK(found);
    } else {
        // no SMP startup available from bootloader,
        // just assume we have a single cpu
        LOG_WARN("smp: missing limine SMP support");
        RETHROW(pcpu_init(1));
        pcpu_init_per_core(0);
    }

    //
    // Continue with the rest of the initialization
    // now that we have a working pcpu data
    //
    init_phys_per_cpu();
    RETHROW(init_tss());
    RETHROW(init_virt());
    RETHROW(init_phys_mappings());
    set_cpu_features();
    switch_page_table();

    init_alloc();

    // load the debug symbols now that we have an allocator
    debug_load_symbols();

    // we need acpi for some early sleep primitives
    RETHROW(init_acpi());

    // we need to calibrate the timer now
    init_tsc();

    // setup the scheduler
    // do that before the SMP startup so it can requests
    // tasks right away
    RETHROW(scheduler_init());

    // perform cpu startup
    if (g_limine_smp_request.response != NULL) {
        struct limine_smp_response* response = g_limine_smp_request.response;

        LOG("smp: Starting CPUs (%d)", g_cpu_count);

        for (size_t i = 0; i < g_cpu_count; i++) {
            if (response->cpus[i]->lapic_id == response->bsp_lapic_id) {
                LOG_DEBUG("smp: \tCPU#%d - LAPIC#%d (BSP)", i, response->cpus[i]->lapic_id);

                // allocate the per-cpu storage now that we know our id
                RETHROW(scheduler_init_per_core());

                m_smp_count++;
            } else {
                // start it up
                response->cpus[i]->extra_argument = i;
                response->cpus[i]->goto_address = smp_entry;

                while (m_smp_count != i + 1) {
                    cpu_relax();
                }
            }
        }

        // wait for smp to finish up
        // TODO: timeout?
        while (m_smp_count != g_cpu_count) {
            cpu_relax();
        }
        LOG_DEBUG("smp: Finished SMP startup");
    }

    // we are about done, create the init thread and queue it
    m_init_thread = thread_create(init_thread_entry, NULL, "init thread");
    scheduler_start_thread(m_init_thread);

    // and we are ready to start the scheduler
    scheduler_start_per_core();

cleanup:
    halt();
}
