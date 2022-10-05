#include "delay.h"
#include "acpi/acpi10.h"
#include "acpi/acpi20.h"

#include <acpi/acpi.h>
#include <mem/mem.h>

#include "arch/intrin.h"

/**
 * The pm timer address
 */
static uint64_t m_pm_timer = 0;
static bool m_pm_timer_ismem = false;

/**
 * The amount of bits in the pm timer
 */
static uint8_t m_pm_timer_bits = 0;

err_t init_delay() {
    err_t err = NO_ERROR;

    // get the fadt
    acpi_descriptor_header_t* fadt = acpi_get_table(ACPI_1_0_FADT_SIGNATURE);
    CHECK(fadt != NULL);

    if (fadt->revision >= ACPI_2_0_FADT_REVISION) {
        acpi_2_0_fadt_t* fadt_2 = (acpi_2_0_fadt_t*)fadt;
        acpi_2_0_generic_address_structure_t* x_tmr = &fadt_2->x_pm_tmr_blk;
        bool x_tmr_supported = x_tmr && (x_tmr->address_space_id == ACPI_2_0_SYSTEM_MEMORY || x_tmr->address_space_id == ACPI_2_0_SYSTEM_IO);
        if (x_tmr_supported) {
            m_pm_timer = x_tmr->address;
            m_pm_timer_ismem = x_tmr->address_space_id == ACPI_2_0_SYSTEM_MEMORY;
            m_pm_timer_bits = (fadt_2->flags & ACPI_2_0_TMR_VAL_EXT) ? 32 : 24;
        } else {
            CHECK(fadt_2->pm_tmr_blk != 0);
            CHECK(fadt_2->pm_tmr_len == 4);
            m_pm_timer = fadt_2->pm_tmr_blk;
            m_pm_timer_bits = (fadt_2->flags & ACPI_2_0_TMR_VAL_EXT) ? 32 : 24;
        }
    } else {
        acpi_1_0_fadt_t* fadt_1 = (acpi_1_0_fadt_t*)fadt;
        CHECK(fadt_1->pm_tmr_blk != 0);
        CHECK(fadt_1->pm_tmr_len == 4);
        m_pm_timer = fadt_1->pm_tmr_blk;
        m_pm_timer_bits = (fadt_1->flags & ACPI_1_0_TMR_VAL_EXT) ? 32 : 24;
    }

    if (m_pm_timer_ismem) TRACE("PM Timer: address 0x%08x, %d bits", m_pm_timer, m_pm_timer_bits);
    else                  TRACE("PM Timer: port %04xh, %d bits", m_pm_timer, m_pm_timer_bits);

cleanup:
    return err;
}

static inline uint32_t read_timer() {
    if (m_pm_timer_ismem) {
        // TODO: support all of GenericAddressStructure
        return *((volatile uint32_t*)PHYS_TO_DIRECT(m_pm_timer));
    } else {
        return __indword(m_pm_timer);
    }
}

void microdelay(uint64_t delay_time) {
    // There are 3.58 ticks per us, so we have to convert the number of microseconds
    // passed in to the number of ticks that need to pass before the timer has expired.
    uint64_t ticks_needed = delay_time * 3;
    ticks_needed += (delay_time * 5) / 10;
    ticks_needed += (delay_time * 8) / 100;

    // calculate the overflows that should happen and the rest that needs to pass
    uint64_t overflow = ticks_needed / (1UL << m_pm_timer_bits);
    uint64_t the_rest = ticks_needed % (1UL << m_pm_timer_bits);

    // read the acpi timer
    uint32_t timer_value = read_timer();

    // need to adjust the values based on the start time
    uint64_t end_value = the_rest + timer_value;

    if (end_value < timer_value) {
        overflow++;
    } else {
        overflow += end_value / (1UL << m_pm_timer_bits);
        end_value = end_value % (1UL << m_pm_timer_bits);
    }

    // Let the timer wrap around as many times as calculated
    uint32_t new_timer_value;
    while (overflow > 0) {
        new_timer_value = read_timer();
        if (new_timer_value < timer_value) {
            overflow--;
        }
        timer_value = new_timer_value;
    }

    // Now wait for the correct number of ticks that need
    // to occur after all the needed overflows
    while (end_value > timer_value) {
        new_timer_value = read_timer();
        if (new_timer_value < timer_value) {
            break;
        }
        timer_value = new_timer_value;
    }
}
