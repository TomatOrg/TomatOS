#include "apic.h"

#include <stdint.h>
#include <lib/except.h>
#include <mem/memory.h>
#include <time/tsc.h>

#define XAPIC_ID                          (*(volatile uint32_t*)(DIRECT_MAP_OFFSET + 0xFEE00000 + 0x20))
#define XAPIC_VERSION                     (*(volatile uint32_t*)(DIRECT_MAP_OFFSET + 0xFEE00000 + 0x30))
#define XAPIC_EOI                         (*(volatile uint32_t*)(DIRECT_MAP_OFFSET + 0xFEE00000 + 0x0b0))
#define XAPIC_ICR_DFR                     (*(volatile uint32_t*)(DIRECT_MAP_OFFSET + 0xFEE00000 + 0x0e0))
#define XAPIC_SPURIOUS_VECTOR             (*(volatile LOCAL_APIC_SVR*)(DIRECT_MAP_OFFSET + 0xFEE00000 + 0x0f0))
#define XAPIC_ICR_LOW                     (*(volatile uint32_t*)(DIRECT_MAP_OFFSET + 0xFEE00000 + 0x300))
#define XAPIC_ICR_HIGH                    (*(volatile uint32_t*)(DIRECT_MAP_OFFSET + 0xFEE00000 + 0x310))
#define XAPIC_LVT_TIMER                   (*(volatile LOCAL_APIC_LVT_TIMER*)(DIRECT_MAP_OFFSET + 0xFEE00000 + 0x320))
#define XAPIC_LVT_LINT0                   (*(volatile uint32_t*)(DIRECT_MAP_OFFSET + 0xFEE00000 + 0x350))
#define XAPIC_LVT_LINT1                   (*(volatile uint32_t*)(DIRECT_MAP_OFFSET + 0xFEE00000 + 0x360))
#define XAPIC_TIMER_INIT_COUNT            (*(volatile uint32_t*)(DIRECT_MAP_OFFSET + 0xFEE00000 + 0x380))
#define XAPIC_TIMER_CURRENT_COUNT         (*(volatile uint32_t*)(DIRECT_MAP_OFFSET + 0xFEE00000 + 0x390))
#define XAPIC_TIMER_DIVIDE_CONFIGURATION  (*(volatile uint32_t*)(DIRECT_MAP_OFFSET + 0xFEE00000 + 0x3E0))

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
        uint32_t spurious_vector : 8;
        uint32_t software_enable : 1;
        uint32_t focus_processor_checking : 1;
        uint32_t : 2;
        uint32_t eoi_broadcast_suppression : 1;
        uint32_t : 19;
    };
    uint32_t packed;
} LOCAL_APIC_SVR;
STATIC_ASSERT(sizeof(LOCAL_APIC_SVR) == sizeof(uint32_t));

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
STATIC_ASSERT(sizeof(LOCAL_APIC_LVT_TIMER) == sizeof(uint32_t));

void init_lapic_per_core(void) {
    // set the spurious vector
    XAPIC_SPURIOUS_VECTOR = (LOCAL_APIC_SVR){
        .spurious_vector = 0xFF,
        .software_enable = 1,
    };

    // make sure timer is disabled
    tsc_set_deadline(0);

    // set the timer, we configure it for tsc deadline
    XAPIC_LVT_TIMER = (LOCAL_APIC_LVT_TIMER){
        .vector = 0x20,
        .timer_mode = 2,
    };
}

void lapic_eoi(void) {
    XAPIC_EOI = 0;
}
