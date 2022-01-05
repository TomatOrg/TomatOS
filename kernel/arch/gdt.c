#include <stdint.h>
#include "gdt.h"

typedef struct gdt {
    uint16_t limit;
    uint64_t* base;
} __attribute__((packed)) gdt_t;

static uint64_t m_gdt_entries[] = {
    0x0000000000000000,
    0x00af9b000000ffff,
    0x00af93000000ffff
};

static void __lgdt(const gdt_t* gdtr) {
    asm volatile ("lgdt %0" : : "m"(*gdtr));
}

void init_gdt() {
    gdt_t gdt = {
        .limit = sizeof(m_gdt_entries) - 1,
        .base = m_gdt_entries
    };
    __lgdt(&gdt);

    asm volatile (
        "movq %%rsp, %%rax\n"
        "pushq $16\n"
        "pushq %%rax\n"
        "pushfq\n"
        "pushq $8\n"
        "pushq $1f\n"
        "iretq\n"
        "1:\n"
        "movw $16, %%ax\n"
        "movw %%ax, %%ss\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        ::: "memory", "rax");
}
