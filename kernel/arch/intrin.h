#pragma once

#define INTRIN_ATTR __attribute__((always_inline, artificial, target("general-regs-only")))

static inline INTRIN_ATTR void _disable(void) {
    __asm__ ("cli");
}

static inline INTRIN_ATTR void _enable(void) {
    __asm__ ("sti");
}

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


static inline INTRIN_ATTR void __invlpg(void *const Address) {
    __asm__ __volatile__("invlpg %[Address]" : : [Address] "m"(*((unsigned char *)(Address))));
}

static inline INTRIN_ATTR unsigned long long __readmsr(const int reg) {
    uint32_t low;
    uint32_t high;
    __asm__ __volatile__("rdmsr" : "=d"(high), "=a"(low) : "c"(reg));
    return (uint64_t)high << 32 | low;
}

static inline INTRIN_ATTR void __writemsr(const unsigned long Register, const unsigned long long Value) {
    __asm__ __volatile__("wrmsr" : : "d"((uint32_t)(Value >> 32)), "a"((uint32_t)Value), "c"(Register));
}

static inline INTRIN_ATTR void __cpuid(int CPUInfo[], const int InfoType) {
    __asm__ __volatile__("cpuid" : "=a"(CPUInfo[0]), "=b"(CPUInfo[1]), "=c"(CPUInfo[2]), "=d"(CPUInfo[3]) : "a"(InfoType));
}

static  inline INTRIN_ATTR void __halt(void) {
    __asm__ ("hlt");
}

static  inline INTRIN_ATTR void __nop(void) {
    __asm__ ("nop");
}

static inline INTRIN_ATTR unsigned long long __readeflags(void) {
    return __builtin_ia32_readeflags_u64();
}

static inline INTRIN_ATTR void _fxsave64(void* p) {
    __builtin_ia32_fxsave64(p);
}

static inline INTRIN_ATTR void _fxrstor64(void* p) {
    __builtin_ia32_fxrstor64(p);
}

#define _rdtsc() __rdtsc()

static inline INTRIN_ATTR void _mm_lfence(void) {
    __atomic_thread_fence(__ATOMIC_RELEASE);
}

#undef INTRIN_ATTR
