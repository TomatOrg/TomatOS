#pragma once

#include <stdint.h>

#define INTRIN_ATTR __attribute__((always_inline, artificial, target("general-regs-only")))

static inline INTRIN_ATTR unsigned char __inbyte(const unsigned short Port) {
    unsigned char byte;
    __asm__ __volatile__("inb %w[Port], %b[byte]" : [byte] "=a"(byte) : [Port] "Nd"(Port));
    return byte;
}

static inline INTRIN_ATTR unsigned short __inword(const unsigned short Port) {
    unsigned short word;
    __asm__ __volatile__("inw %w[Port], %w[word]" : [word] "=a"(word) : [Port] "Nd"(Port));
    return word;
}

static inline INTRIN_ATTR unsigned long __indword(const unsigned short Port) {
    unsigned long dword;
    __asm__ __volatile__("inl %w[Port], %k[dword]" : [dword] "=a"(dword) : [Port] "Nd"(Port));
    return dword;
}

static inline INTRIN_ATTR void __outbyte(unsigned short const Port, const unsigned char Data) {
    __asm__ __volatile__("outb %b[Data], %w[Port]" : : [Port] "Nd"(Port), [Data] "a"(Data));
}

static inline INTRIN_ATTR void __outword(unsigned short const Port, const unsigned short Data) {
    __asm__ __volatile__("outw %w[Data], %w[Port]" : : [Port] "Nd"(Port), [Data] "a"(Data));
}

static inline INTRIN_ATTR void __outdword(unsigned short const Port, const unsigned long Data) {
    __asm__ __volatile__("outl %k[Data], %w[Port]" : : [Port] "Nd"(Port), [Data] "a"(Data));
}

static inline INTRIN_ATTR void cpu_relax() {
    __builtin_ia32_pause();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Control register access
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static inline INTRIN_ATTR void __invlpg(void* m) {
    asm volatile ( "invlpg (%0)" : : "b"(m) : "memory" );
}

static inline INTRIN_ATTR unsigned long __readcr0(void) {
    unsigned long value;
    __asm__ __volatile__("mov %%cr0, %[value]" : [value] "=q"(value));
    return value;
}

static inline INTRIN_ATTR unsigned long __readcr2(void) {
    unsigned long value;
    __asm__ __volatile__("mov %%cr2, %[value]" : [value] "=q"(value));
    return value;
}

static inline INTRIN_ATTR unsigned long __readcr3(void) {
    unsigned long value;
    __asm__ __volatile__("mov %%cr3, %[value]" : [value] "=q"(value));
    return value;
}

static inline INTRIN_ATTR unsigned long __readcr4(void) {
    unsigned long value;
    __asm__ __volatile__("mov %%cr4, %[value]" : [value] "=q"(value));
    return value;
}

static inline INTRIN_ATTR unsigned long __readcr8(void) {
    unsigned long value;
    __asm__ __volatile__("mov %%cr8, %[value]" : [value] "=q"(value));
    return value;
}

static inline INTRIN_ATTR void __writecr0(const unsigned long long Data) {
    __asm__ __volatile__("mov %[Data], %%cr0" : : [Data] "q"(Data) : "memory");
}

static inline INTRIN_ATTR void __writecr3(const unsigned long long Data) {
    __asm__ __volatile__("mov %[Data], %%cr3" : : [Data] "q"(Data) : "memory");
}

static inline INTRIN_ATTR void __writecr4(const unsigned long long Data) {
    __asm__ __volatile__("mov %[Data], %%cr4" : : [Data] "q"(Data) : "memory");
}

static inline INTRIN_ATTR void __writecr8(const unsigned long long Data) {
    __asm__ __volatile__("mov %[Data], %%cr8" : : [Data] "q"(Data) : "memory");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MWAIT
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static inline INTRIN_ATTR void __mwait(uintptr_t eax, uintptr_t ecx) {
    __asm__ __volatile__("mwait" : : "a"(eax), "c"(ecx));
}

static inline INTRIN_ATTR void __monitor(uintptr_t eax, uintptr_t ecx, uintptr_t edx) {
    __asm__ __volatile__("monitor" : : "a"(eax), "c"(ecx), "d"(edx));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MSR access
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define MSR_IA32_FS_BASE  0xC0000100

#define MSR_IA32_APIC_BASE 0x0000001B

typedef union {
    struct {
        uint64_t : 8;
        uint64_t bsp : 1;
        uint64_t : 1;
        uint64_t extd : 1;
        uint64_t en : 1;
        uint64_t apic_base : 52;
    };
    uint64_t packed;
} MSR_IA32_APIC_BASE_REGISTER;

#define MSR_IA32_TSC_DEADLINE  0x000006E0

static inline INTRIN_ATTR void __wrmsr(uint32_t index, uint64_t value) {
    uint32_t low_data = value;
    uint32_t high_data = value >> 32;
    __asm__ __volatile__("wrmsr" : : "c"(index), "a"(low_data), "d"(high_data));
}

static inline INTRIN_ATTR uint64_t __rdmsr(uint32_t index) {
    uint32_t low_data;
    uint32_t high_data;
    __asm__ __volatile__("rdmsr" : "=a"(low_data), "=d"(high_data) : "c"(index));
    return low_data | ((uint64_t)high_data << 32);
}
