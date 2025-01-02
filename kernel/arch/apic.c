#include "apic.h"

#include <limine_requests.h>
#include <stdbool.h>
#include <stdint.h>
#include <lib/except.h>
#include <mem/memory.h>
#include <mem/virt.h>
#include <time/tsc.h>

#include "intrin.h"
#include "smp.h"

#define XAPIC_ID_OFFSET                          0x20
#define XAPIC_VERSION_OFFSET                     0x30
#define XAPIC_EOI_OFFSET                         0x0b0
#define XAPIC_ICR_DFR_OFFSET                     0x0e0
#define XAPIC_SPURIOUS_VECTOR_OFFSET             0x0f0
#define XAPIC_ICR_LOW_OFFSET                     0x300
#define XAPIC_ICR_HIGH_OFFSET                    0x310
#define XAPIC_LVT_TIMER_OFFSET                   0x320
#define XAPIC_LVT_LINT0_OFFSET                   0x350
#define XAPIC_LVT_LINT1_OFFSET                   0x360
#define XAPIC_TIMER_INIT_COUNT_OFFSET            0x380
#define XAPIC_TIMER_CURRENT_COUNT_OFFSET         0x390
#define XAPIC_TIMER_DIVIDE_CONFIGURATION_OFFSET  0x3E0

#define X2APIC_MSR_BASE_ADDRESS  0x800
#define X2APIC_MSR_ICR_ADDRESS   0x830

#define LOCAL_APIC_DELIVERY_MODE_FIXED            0
#define LOCAL_APIC_DELIVERY_MODE_LOWEST_PRIORITY  1
#define LOCAL_APIC_DELIVERY_MODE_SMI              2
#define LOCAL_APIC_DELIVERY_MODE_NMI              4
#define LOCAL_APIC_DELIVERY_MODE_INIT             5
#define LOCAL_APIC_DELIVERY_MODE_STARTUP          6
#define LOCAL_APIC_DELIVERY_MODE_EXTINT           7

#define LOCAL_APIC_DESTINATION_SHORTHAND_NO_SHORTHAND        0
#define LOCAL_APIC_DESTINATION_SHORTHAND_SELF                1
#define LOCAL_APIC_DESTINATION_SHORTHAND_ALL_INCLUDING_SELF  2
#define LOCAL_APIC_DESTINATION_SHORTHAND_ALL_EXCLUDING_SELF  3

typedef union {
    struct {
        uint32_t SpuriousVector          : 8;  ///< Spurious Vector.
        uint32_t SoftwareEnable          : 1;  ///< APIC Software Enable/Disable.
        uint32_t FocusProcessorChecking  : 1;  ///< Focus Processor Checking.
        uint32_t Reserved0               : 2;  ///< Reserved.
        uint32_t EoiBroadcastSuppression : 1;  ///< EOI-Broadcast Suppression.
        uint32_t Reserved1               : 19; ///< Reserved.
    };
    uint32_t packed;
} LOCAL_APIC_SVR;

typedef union {
    struct {
        uint32_t vector : 8;
        uint32_t delivery_mode : 3;
        uint32_t : 1;
        uint32_t delivery_status : 1;
        uint32_t input_pin_polarity : 1;
        uint32_t remote_irr : 1;
        uint32_t trigger_mode : 1;
        uint32_t mask : 1;
        uint32_t : 15;
    };
    uint32_t packed;
} LOCAL_APIC_LVT_LINT;

typedef union {
    struct {
        uint32_t vector : 8;
        uint32_t : 4;
        uint32_t delivery_status : 1;
        uint32_t : 3;
        uint32_t mask : 1;
        uint32_t timer_mode : 2;
        uint32_t : 13;
    };
    uint32_t packed;
} LOCAL_APIC_LVT_TIMER;

/**
 * Are we using x2APIC mode
 */
static bool m_x2apic_mode = false;

/**
 * The xAPIC base, when using xAPIC mode
 */
static uint8_t* m_xapic_base = NULL;

err_t init_lapic(void) {
    err_t err = NO_ERROR;

    // check the apic state
    MSR_IA32_APIC_BASE_REGISTER apic_base = { .packed = __rdmsr(MSR_IA32_APIC_BASE) };
    CHECK(apic_base.en);
    if (apic_base.extd) {
        m_x2apic_mode = true;
        LOG_INFO("apic: using x2apic");

    } else {
        m_x2apic_mode = false;
        LOG_INFO("apic: using xapic");

        // calculate the address
        m_xapic_base = PHYS_TO_DIRECT(apic_base.apic_base << 12);

        // make sure the apic is mapped properly
        RETHROW(virt_map_page(apic_base.apic_base << 12, (uintptr_t)m_xapic_base, MAP_PERM_W));
    }

cleanup:
    return err;
}

static void lapic_write(size_t offset, uint32_t value) {
    if (m_x2apic_mode) {
        asm("" ::: "memory");
        __wrmsr((offset >> 4) + X2APIC_MSR_BASE_ADDRESS, value);
    } else {
        *((uint32_t*)(m_xapic_base + offset)) = value;
    }
}

void init_lapic_per_core(void) {
    // set the spurious vector
    LOCAL_APIC_SVR svr = {
        .SpuriousVector = 0xFF,
        .SoftwareEnable = 1
    };
    lapic_write(XAPIC_SPURIOUS_VECTOR_OFFSET, svr.packed);

    // make sure timer is disabled
    tsc_disable_timeout();

    // set the timer, we configure it for tsc deadline
    LOCAL_APIC_LVT_TIMER timer = {
        .vector = 0x20,
        .timer_mode = 2
    };
    lapic_write(XAPIC_LVT_TIMER_OFFSET, timer.packed);
}

void lapic_eoi(void) {
    lapic_write(XAPIC_EOI_OFFSET, 0);
}
