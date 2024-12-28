#include "apic.h"

#include <stdint.h>
#include <lib/except.h>
#include <mem/memory.h>
#include <time/tsc.h>



void init_lapic_per_core(void) {
    // // set the spurious vector
    // XAPIC_SPURIOUS_VECTOR = (LOCAL_APIC_SVR){
    //     .spurious_vector = 0xFF,
    //     .software_enable = 1,
    // };
    //
    // // make sure timer is disabled
    // tsc_disable_timeout();
    //
    // // set the timer, we configure it for tsc deadline
    // XAPIC_LVT_TIMER = (LOCAL_APIC_LVT_TIMER){
    //     .vector = 0x20,
    //     .timer_mode = 2,
    // };
}

void lapic_eoi(void) {
    // XAPIC_EOI = 0;
}
