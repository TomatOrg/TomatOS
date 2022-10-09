#pragma once

#include "util/except.h"

#define GDT_CODE offsetof(gdt_entries_t, code)
#define GDT_DATA offsetof(gdt_entries_t, data)
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

void init_gdt();

err_t init_tss();
