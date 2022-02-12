#include "delay.h"
#include "acpi/acpi10.h"

#include <arch/intrin.h>
#include <acpi/acpi.h>


/**
 * The pm timer address
 */
static uint16_t m_pm_timer = 0;

/**
 * The amount of bits in the pm timer
 */
static uint8_t m_pm_timer_bits = 0;

err_t init_delay() {
    err_t err = NO_ERROR;

    // get the fadt
    acpi_1_0_fadt_t* fadt = acpi_get_table(EFI_ACPI_1_0_FADT_SIGNATURE);
    CHECK(fadt != NULL);

    // make sure this has a valid value
    CHECK(fadt->pm_tmr_len == 4);
    CHECK(fadt->pm_tmr_blk != 0);

    // set the values
    m_pm_timer = fadt->pm_tmr_blk;
    m_pm_timer_bits = (fadt->flags & ACPI_1_0_TMR_VAL_EXT) ? 32 : 24;
    TRACE("PM Timer: %04xh, %d bits", m_pm_timer, m_pm_timer_bits);

cleanup:
    return err;
}

void microdelay(uint64_t delay_time) {
    // There are 3.58 ticks per us, so we have to convert the number of microseconds
    // passed in to the number of ticks that need to pass before the timer has expired.
    uint64_t ticks_needed = delay_time * 3;
    ticks_needed += (delay_time * 5) / 10;
    ticks_needed += (delay_time * 8) / 100;

    // calculate the overflows that should happen and the rest that needs to pass
    uint64_t overflow = ticks_needed / (1 << m_pm_timer_bits);
    uint64_t the_rest = ticks_needed % (1 << m_pm_timer_bits);

    // read the acpi timer
    uint32_t timer_value = __indword(m_pm_timer);

    // need to adjust the values based on the start time
    uint64_t end_value = the_rest + timer_value;

    if (end_value < timer_value) {
        overflow++;
    } else {
        overflow += end_value / (1 << m_pm_timer_bits);
        end_value = end_value % (1 << m_pm_timer_bits);
    }

    // Let the timer wrap around as many times as calculated
    uint32_t new_timer_value;
    while (overflow > 0) {
        new_timer_value = __indword(m_pm_timer);
        if (new_timer_value < timer_value) {
            overflow--;
        }
        timer_value = new_timer_value;
    }

    // Now wait for the correct number of ticks that need
    // to occur after all the needed overflows
    while (end_value > timer_value) {
        new_timer_value = __indword(m_pm_timer);
        if (new_timer_value < timer_value) {
            break;
        }
        timer_value = new_timer_value;
    }
}
