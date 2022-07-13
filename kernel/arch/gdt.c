#include "gdt.h"
#include "sync/spinlock.h"
#include "mem/phys.h"

#include <util/defs.h>

#include <stdint.h>

#define GDT_TSS offsetof(gdt_entries_t, tss)

typedef struct gdt64_entry {
    uint16_t limit;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} PACKED gdt64_entry_t;

typedef struct tss64_entry {
    uint16_t length;
    uint16_t low;
    uint8_t mid;
    uint8_t flags1;
    uint8_t flags2;
    uint8_t high;
    uint32_t upper32;
    uint32_t reserved;
} PACKED tss64_entry_t;

typedef struct gdt_entries {
    gdt64_entry_t null;
    gdt64_entry_t code;
    gdt64_entry_t data;
    tss64_entry_t tss;
} PACKED gdt_entries_t;

typedef struct gdt {
    uint16_t size;
    gdt_entries_t* entries;
} PACKED gdt_t;

typedef struct tss64 {
    uint32_t reserved_1;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved_2;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved_3;
    uint32_t iopb_offset;
} PACKED tss64_t;

static gdt_entries_t m_entries = {
    {   // null descriptor
        .limit          = 0x0000,
        .base_low       = 0x0000,
        .base_mid       = 0x00,
        .access         = 0b00000000,
        .granularity    = 0b00000000,
        .base_high      = 0x00
    },
    {   // kernel code
        .limit          = 0x0000,
        .base_low       = 0x0000,
        .base_mid       = 0x00,
        .access         = 0b10011010,
        .granularity    = 0b00100000,
        .base_high      = 0x00
    },
    {   // kernel data
        .limit          = 0x0000,
        .base_low       = 0x0000,
        .base_mid       = 0x00,
        .access         = 0b10010010,
        .granularity    = 0b00000000,
        .base_high      = 0x00
    },
    {   // TSS
        .length         = 0,
        // Will be filled by the init function
        .low            = 0,
        .mid            = 0,
        .high           = 0,
        .upper32        = 0,
        .flags1         = 0b10001001,
        .flags2         = 0b00000000,
        .reserved       = 0
    }
};

static gdt_t m_gdt = {
    .size = sizeof(gdt_entries_t) - 1,
    .entries = &m_entries
};

void init_gdt() {
    asm volatile (
        "lgdt %0\n"
        "movq %%rsp, %%rax\n"
        "pushq $16\n"
        "pushq %%rax\n"
        "pushfq\n"
        "pushq $8\n"
        "lea 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "iretq\n"
        "1:\n"
        "movw $16, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        :
        : "m"(m_gdt)
        : "memory", "rax");
}

/**
 * We are using the same gdt entry for each core, so we can't
 * have two cores loading it at the same time
 */
static spinlock_t m_tss_lock = INIT_SPINLOCK();

err_t init_tss() {
    err_t err = NO_ERROR;

    // we need to allocate this since it has to continue being alive
    tss64_t* tss = palloc(sizeof(tss64_t));
    CHECK_ERROR(tss != NULL, ERROR_OUT_OF_MEMORY);

    // set the ists
    tss->ist1 = (uintptr_t)(palloc(SIZE_8KB) + SIZE_8KB - 16);
    tss->ist2 = (uintptr_t)(palloc(SIZE_8KB) + SIZE_8KB - 16);
    tss->ist3 = (uintptr_t)(palloc(SIZE_8KB) + SIZE_8KB - 16);
    tss->ist4 = (uintptr_t)(palloc(SIZE_8KB) + SIZE_8KB - 16);
    CHECK_ERROR(tss->ist1 != 0, ERROR_OUT_OF_MEMORY);
    CHECK_ERROR(tss->ist2 != 0, ERROR_OUT_OF_MEMORY);
    CHECK_ERROR(tss->ist3 != 0, ERROR_OUT_OF_MEMORY);
    CHECK_ERROR(tss->ist4 != 0, ERROR_OUT_OF_MEMORY);

    spinlock_lock(&m_tss_lock);
    m_gdt.entries->tss.length = sizeof(tss64_t);
    m_gdt.entries->tss.low = (uint16_t)(uintptr_t)tss;
    m_gdt.entries->tss.mid = (uint8_t)((uintptr_t)tss >> 16u);
    m_gdt.entries->tss.high = (uint8_t)((uintptr_t)tss >> 24u);
    m_gdt.entries->tss.upper32 = (uint32_t)((uintptr_t)tss >> 32u);
    m_gdt.entries->tss.flags1 = 0b10001001;
    m_gdt.entries->tss.flags2 = 0b00000000;
    asm volatile ("ltr %%ax" : : "a"(GDT_TSS) : "memory");
    spinlock_unlock(&m_tss_lock);

cleanup:
    return err;
}
