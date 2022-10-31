#include "apic.h"

#include "intrin.h"
#include "msr.h"
#include "time/tsc.h"
#include "cpuid.h"
#include "irq/irq.h"
#include "thread/cpu_local.h"

#include <util/defs.h>
#include <mem/mem.h>

#include <stdint.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define XAPIC_ID_OFFSET                             0x20
#define XAPIC_VERSION_OFFSET                        0x30
#define XAPIC_EOI_OFFSET                            0x0b0
#define XAPIC_ICR_DFR_OFFSET                        0x0e0
#define XAPIC_SPURIOUS_VECTOR_OFFSET                0x0f0
#define XAPIC_ICR_LOW_OFFSET                        0x300
#define XAPIC_ICR_HIGH_OFFSET                       0x310
#define XAPIC_LVT_TIMER_OFFSET                      0x320
#define XAPIC_LVT_LINT0_OFFSET                      0x350
#define XAPIC_LVT_LINT1_OFFSET                      0x360
#define XAPIC_TIMER_INIT_COUNT_OFFSET               0x380
#define XAPIC_TIMER_CURRENT_COUNT_OFFSET            0x390
#define XAPIC_TIMER_DIVIDE_CONFIGURATION_OFFSET     0x3E0

#define X2APIC_MSR_BASE_ADDRESS                     0x800
#define X2APIC_MSR_ICR_ADDRESS                      0x830

typedef enum lapic_delivery_mode {
    DELIVERY_MODE_FIXED = 0,
    DELIVERY_MODE_LOWEST_PRIORITY = 1,
    DELIVERY_MODE_SMI = 2,
    DELIVERY_MODE_NMI = 3,
    DELIVERY_MODE_INIT = 4,
    DELIVERY_MODE_STARTUP = 5,
    DELIVERY_MODE_EXTINT = 6,
} lapic_delivery_mode_t;

typedef enum lapic_destination_shorthand {
    DESTINATION_SHORTHAND_NONE = 0,
    DESTINATION_SHORTHAND_SELF = 1,
    DESTINATION_SHORTHAND_ALL_INCLUDING_SELF = 2,
    DESTINATION_SHORTHAND_ALL_EXCLUDING_SELF = 3,
} lapic_destination_shorthand_t;

typedef union lapic_version {
    struct {
        uint32_t version : 8;
        uint32_t _reserved0 : 8;
        uint32_t max_lvt_entry : 8;
        uint32_t eoi_broadcast_suppression : 1;
        uint32_t _reserved1 : 7;
    };
    uint32_t packed;
} PACKED lapic_version_t;
STATIC_ASSERT(sizeof(lapic_version_t) == sizeof(uint32_t));

typedef union lapic_icr_low {
    struct {
        uint32_t vector : 8;
        uint32_t delivery_mode : 3;
        uint32_t destination_mode : 1;
        uint32_t delivery_status : 1;
        uint32_t _reserved0 : 1;
        uint32_t level : 1;
        uint32_t trigger_mode : 1;
        uint32_t _reserved1 : 2;
        uint32_t destination_shorthand : 2;
        uint32_t _reserved2 : 12;
    };
    uint32_t packed;
} PACKED lapic_icr_low_t;
STATIC_ASSERT(sizeof(lapic_icr_low_t) == sizeof(uint32_t));

typedef union lapic_icr_high {
    struct {
        uint32_t _reserved0 : 24;
        uint32_t destination : 8;
    };
    uint32_t packed;
} PACKED lapic_icr_high_t;
STATIC_ASSERT(sizeof(lapic_icr_high_t) == sizeof(uint32_t));

typedef union lapic_svr {
    struct {
        uint32_t spurious_vector : 8;
        uint32_t software_enable : 1;
        uint32_t focus_processor_checking : 1;
        uint32_t _reserved0 : 2;
        uint32_t eoi_broadcast_supression : 1;
        uint32_t _reserved1 : 19;
    };
    uint32_t packed;
} PACKED lapic_svr_t;
STATIC_ASSERT(sizeof(lapic_svr_t) == sizeof(uint32_t));

typedef union lapic_dcr {
    struct {
        uint32_t divide_value_low : 2;
        uint32_t _reserved0 : 1;
        uint32_t divide_value_high : 1;
        uint32_t _reserved : 28;
    };
    uint32_t packed;
} PACKED lapic_dcr_t;
STATIC_ASSERT(sizeof(lapic_dcr_t) == sizeof(uint32_t));

typedef union lapic_lvt_timer {
    struct {
        uint32_t vector : 8;
        uint32_t _reserved0 : 4;
        uint32_t delivery_status : 1;
        uint32_t _reserved1 : 3;
        uint32_t mask : 1;
        uint32_t timer_mode : 1;
        uint32_t tsc_deadline : 1;
        uint32_t _reserved2 : 13;
    };
    uint32_t packed;
} PACKED lapic_lvt_timer_t;
STATIC_ASSERT(sizeof(lapic_lvt_timer_t) == sizeof(uint32_t));

typedef union lapic_lvt_lint {
    struct {
        uint32_t vector : 8;
        uint32_t delivery_mode : 3;
        uint32_t _reserved0 : 1;
        uint32_t delivery_status : 1;
        uint32_t input_pin_polarity : 1;
        uint32_t remote_irr : 1;
        uint32_t trigger_mode : 1;
        uint32_t mask : 1;
        uint32_t _reserved1 : 15;
    };
    uint32_t packed;
} lapic_lvt_lint_t;
STATIC_ASSERT(sizeof(lapic_lvt_lint_t) == sizeof(uint32_t));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * The local apic base address
 * TODO: hardcode it?
 */
static uintptr_t m_local_apic_base = -1;

/**
 * Read from a lapic register
 */
static uint32_t lapic_read(uint32_t offset) {
    return *(volatile uint32_t*)(m_local_apic_base + offset);
}

/**
 * Write to a lapic register
 */
static void lapic_write(uint32_t offset, uint32_t value) {
    *(volatile uint32_t*)(m_local_apic_base + offset) = value;
}

void early_init_apic() {
    // enable the APIC globally
    msr_apic_base_t apic_base_reg = { .packed = __readmsr(MSR_IA32_APIC_BASE) };
    apic_base_reg.en = 1;
    apic_base_reg.extd = 0;
    __writemsr(MSR_IA32_APIC_BASE, apic_base_reg.packed);

    // figure the APIC base
    uintptr_t apic_base = apic_base_reg.apic_base << 12;
    m_local_apic_base = (uintptr_t)PHYS_TO_DIRECT(apic_base);
}

/**
 * Did we already map it
 */
static bool m_mapped_lapic = false;

err_t init_apic() {
    err_t err = NO_ERROR;

    if (!m_mapped_lapic) {
        // not mapped, this is BSP
        m_mapped_lapic = true;
        CHECK_AND_RETHROW(vmm_map(DIRECT_TO_PHYS(m_local_apic_base), (void*)m_local_apic_base, 1,
                                  MAP_WRITE));
    }

    // enable lapic by configuring the svr
    lapic_svr_t svr = {
        .software_enable = 1,
        .spurious_vector = IRQ_SPURIOUS,
    };
    lapic_write(XAPIC_SPURIOUS_VECTOR_OFFSET, svr.packed);

    // validate tsc deadline is supported
    cpuid_version_info_ecx_t version_info_ecx;
    cpuid(CPUID_VERSION_INFO, NULL, NULL, &version_info_ecx.packed, NULL);
    CHECK(version_info_ecx.TSC_Deadline);

    // configure for preempt by default
    lapic_set_preempt();

    // TODO: program wires

cleanup:
    return err;
}

static void send_ipi(lapic_icr_low_t low, uint32_t apic_id) {
    // disable interrupts when doing apic stuff
    bool ints = __readeflags() & BIT9 ? true : false;
    _disable();

    // wait for delivery status to clear
    lapic_icr_low_t current_low;
    do {
        current_low.packed = lapic_read(XAPIC_ICR_LOW_OFFSET);
    } while (current_low.delivery_status != 0);

    // write the regs in correct order (low offset will trigger
    // so do that last)
    lapic_write(XAPIC_ICR_HIGH_OFFSET, apic_id << 24);
    lapic_write(XAPIC_ICR_LOW_OFFSET, low.packed);

    // wait for delivery status to clear again
    do {
        current_low.packed = lapic_read(XAPIC_ICR_LOW_OFFSET);
    } while (current_low.delivery_status != 0);

    // enable interrupts again if needed
    if (ints) {
        _enable();
    }
}

void lapic_eoi() {
    lapic_write(XAPIC_EOI_OFFSET, 0);
}

void lapic_send_ipi(uint8_t vector, int apic) {
    lapic_icr_low_t low = {
        .vector = vector,
        .delivery_mode = DELIVERY_MODE_FIXED,
    };
    send_ipi(low, apic);
}

void lapic_send_ipi_lowest_priority(uint8_t vector) {
    lapic_icr_low_t low = {
        .vector = vector,
        .delivery_mode = DELIVERY_MODE_LOWEST_PRIORITY,
    };
    send_ipi(low, 0);
}

size_t get_apic_id() {
    return lapic_read(XAPIC_ID_OFFSET) >> 24;
}

void lapic_set_wakeup() {
    // enable the local timer
    lapic_lvt_timer_t timer = {
        .vector = IRQ_WAKEUP,
        .tsc_deadline = 1,
    };
    lapic_write(XAPIC_LVT_TIMER_OFFSET, timer.packed);

    // serialize the lapic write
    _mm_mfence();
}

void lapic_set_preempt() {
    // enable the local timer
    lapic_lvt_timer_t timer = {
        .vector = IRQ_PREEMPT,
        .tsc_deadline = 1,
    };
    lapic_write(XAPIC_LVT_TIMER_OFFSET, timer.packed);

    // serialize the lapic write
    _mm_mfence();
}

void lapic_set_timeout(uint64_t microseconds) {
    __writemsr(MSR_IA32_TSC_DEADLINE, __builtin_ia32_rdtsc() + microseconds * get_tsc_freq());
}

void lapic_set_deadline(uint64_t microseconds) {
    __writemsr(MSR_IA32_TSC_DEADLINE, microseconds * get_tsc_freq());

}
