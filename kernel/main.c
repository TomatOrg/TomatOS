#include <cpuid.h>
#include <limine_requests.h>

#include "limine.h"
#include "debug/log.h"
#include "arch/gdt.h"
#include "arch/intr.h"
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
#include <thread/pcpu.h>
#include <thread/scheduler.h>
#include <time/tsc.h>

#include <tomatodotnet/tdn.h>
#include <tomatodotnet/jit/jit.h>

#include "time/timer.h"

/**
 * The init thread
 */
static thread_t* m_init_thread;

static struct limine_file* get_module_by_name(const char* name) {
    struct limine_module_response* response = g_limine_module_request.response;
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

     TRACE("Init thread started");

     // initialize the garbage collector
     gc_init();

    // setup the tdn configuration
    tdn_config_t* config = tdn_get_config();
    config->jit_verify_trace = false;
    config->jit_emit_trace = false;
    config->jit_optimize = true;
    config->jit_inline = true;

     // first load the corelib
     struct limine_file* corelib = get_module_by_name("/System.Private.CoreLib.dll");
     CHECK(corelib != NULL, "Failed to find corelib");
     TDN_RETHROW(tdn_load_assembly_from_memory(corelib->address, corelib->size, NULL));

    // load the kernel itself
    struct limine_file* kernel = get_module_by_name("/Tomato.Kernel.dll");
    CHECK(kernel != NULL, "Failed to find kernel");
    RuntimeAssembly kernel_assembly;
    TDN_RETHROW(tdn_load_assembly_from_memory(kernel->address, kernel->size, &kernel_assembly));

    // jit the entry point and call it
    TDN_RETHROW(tdn_jit_method(kernel_assembly->EntryPoint));
    int (*entry_point)(void) = kernel_assembly->EntryPoint->MethodPtr;
    int status = entry_point();
    CHECK(status == 0, "Managed kernel returned non-zero status %d", status);

cleanup:
     if (IS_ERROR(err)) {
         ERROR("Can't continue loading the OS");
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

typedef struct xcr0_feature {
    const char* name;
    bool enable;
    bool required;
} xcr0_feature_t;

/**
 * The features that we support and want to enable if supported
 */
static const xcr0_feature_t m_xcr0_features[] = {
    [0] = { "x87", true, true },
    [1] = { "SSE", true, true },
    [2] = { "AVX", true, true },
    [3] = { "MPX[BNDREG]", false, false },
    [4] = { "MPX[BNDCSR]", false, false },
    [5] = { "AVX-512[OPMASK]", false, false },
    [6] = { "AVX-512[ZMM_Hi256]", false, false },
    [7] = { "AVX-512[Hi16_ZMM]", false, false },
    [8] = { "PT", false, false },
    [9] = { "PKRU", false, false },
    [10] = { "PASID", false, false },
    [11] = { "CET[U]", false, false },
    [12] = { "CET[S]", false, false },
    [13] = { "HDC", false, false },
    [14] = { "UINTR", false, false },
    [15] = { "LBR", false, false },
    [16] = { "HWP", false, false },
    [17] = { "AMX[TILECFG]", false, false },
    [18] = { "AMX[XTILEDATA]", false, false },
    [19] = { "APX", false, false },
};

static void set_extended_state_features(void) {
    static bool first = true;
    static uint32_t first_xcr0 = 0;
    uint32_t a, b, c, d;

    // ensure we have xsave (for the basic support sutff)
    __cpuid(1, a, b, c, d);
    ASSERT(c & bit_XSAVE, "Missing support for xsave");

    // we are going to force xsaveopt for now
    __cpuid_count(0xD, 1, a, b, c, d);
    ASSERT(a & bit_XSAVEOPT, "Missing support for xsaveopt");

    // enable/disable extended features or something
    if (first) TRACE("extended state:");
    __cpuid_count(0xD, 0, a, b, c, d);
    uint64_t xcr0 = 0;
    uint64_t features = a | ((uint64_t)d << 32);
    for (int i = 0; i < ARRAY_LENGTH(m_xcr0_features); i++) {
        const xcr0_feature_t* feature = &m_xcr0_features[i];
        uint64_t bit = 1 << i;
        if ((features & bit) != 0) {
            if (feature->enable) {
                xcr0 |= bit;
                if (first) TRACE("\t- %s [enabling]", feature->name);
            } else {
                if (first) TRACE("\t- %s", feature->name);
            }
        } else {
            ASSERT(!feature->required, "Missing required feature %s", feature->name);
        }
    }

    // ensure that we have a consistent feature view
    if (first) {
        first_xcr0 = xcr0;
    } else {
        ASSERT(first_xcr0 == xcr0);
    }
    __builtin_ia32_xsetbv(0, xcr0);

    if (first) {
        __cpuid_count(0xD, 0, a, b, c, d);
        TRACE("extended state size is %d bytes", b);
    }

    first = false;
}

static void set_cpu_features(void) {
    // PG/PE - required for long mode
    // MP - required for SSE
    __writecr0(CR0_PG | CR0_PE | CR0_MP);

    // PAE - required for long mode
    // OSFXSR/OSXMMEXCPT - required for SSE
    __writecr4(CR4_PAE | CR4_OSFXSR | CR4_OSXSAVE | CR4_OSXMMEXCPT);

    set_extended_state_features();
}

static void halt() {
    irq_disable();
    for (;;) {
        asm("hlt");
    }
}

static void smp_entry(struct limine_mp_info* info) {
    err_t err = NO_ERROR;

    //
    // Start by setting the proper CPU context
    //
    init_gdt();
    init_idt();
    set_cpu_features();
    switch_page_table();

    TRACE("smp: \tCPU#%lu - LAPIC#%d", info->extra_argument, info->lapic_id);

    //
    // And now setup the per-cpu
    //
    pcpu_init_per_core(info->extra_argument);
    init_phys_per_cpu();
    RETHROW(init_tss());

    // and now we can init
    init_lapic_per_core();
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
    TRACE("------------------------------------------------------------------------------------------------------------");
    TRACE("TomatOS");
    TRACE("------------------------------------------------------------------------------------------------------------");
    limine_check_revision();

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
    if (g_limine_mp_request.response != NULL) {
        g_cpu_count = g_limine_mp_request.response->cpu_count;

        // allocate all the storage needed
        RETHROW(pcpu_init(g_limine_mp_request.response->cpu_count));

        // now find the correct cpu id and set it
        bool found = false;
        for (int i = 0; i < g_limine_mp_request.response->cpu_count; i++) {
            if (g_limine_mp_request.response->cpus[i]->lapic_id == g_limine_mp_request.response->bsp_lapic_id) {
                pcpu_init_per_core(i);
                found = true;
                break;
            }
        }
        CHECK(found);
    } else {
        // no SMP startup available from bootloader,
        // just assume we have a single cpu
        WARN("smp: missing limine SMP support");
        RETHROW(pcpu_init(1));
        pcpu_init_per_core(0);
    }
    init_logging();

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
    RETHROW(init_acpi_tables());

    // we need to calibrate the timer now
    init_tsc();

    // setup the scheduler
    // do that before the SMP startup so it can requests
    // tasks right away
    RETHROW(scheduler_init());

    // perform cpu startup
    CHECK(g_limine_mp_request.response != NULL);
    struct limine_mp_response* response = g_limine_mp_request.response;

    RETHROW(init_lapic());

    TRACE("smp: Starting CPUs (%zu)", g_cpu_count);

    for (size_t i = 0; i < g_cpu_count; i++) {
        if (response->cpus[i]->lapic_id == response->bsp_lapic_id) {
            TRACE("smp: \tCPU#%zu - LAPIC#%d (BSP)", i, response->cpus[i]->lapic_id);

            // allocate the per-cpu storage now that we know our id
            init_lapic_per_core();
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
    TRACE("smp: Finished SMP startup");

    // we are about done, create the init thread and queue it
    m_init_thread = thread_create(init_thread_entry, NULL, "init thread");
    scheduler_wakeup_thread(m_init_thread);

    // and we are ready to start the scheduler
    scheduler_start_per_core();

cleanup:
    halt();
}
