/* ===-------- intrin.h ---------------------------------------------------===
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 *===-----------------------------------------------------------------------===
 */

#ifndef __INTRIN_H
#define __INTRIN_H

/* First include the standard intrinsics. */
#if defined(__i386__) || defined(__x86_64__)
#include <x86intrin.h>
#endif

#if defined(__arm__)
#include <armintr.h>
#endif

#if defined(__aarch64__)
#include <arm64intr.h>
#endif

/* For the definition of jmp_buf. */
#if __STDC_HOSTED__
#include <setjmp.h>
#endif

/* Define the default attributes for the functions in this file. */
#define __DEFAULT_FN_ATTRS __attribute__((__always_inline__, __nodebug__))

#if __x86_64__
#define __LPTRINT_TYPE__ __int64
#else
#define __LPTRINT_TYPE__ long
#endif

#ifdef __cplusplus
extern "C" {
#endif

static __inline__ __attribute__((always_inline, artificial)) void _disable(void) {
    __asm__("cli");
}

static __inline__ __attribute__((always_inline, artificial)) void _enable(void) {
    __asm__("sti");
}

static __inline__ __attribute__((always_inline, artificial)) unsigned char __inbyte(
        const unsigned short Port) {
    unsigned char byte;
    __asm__ __volatile__("inb %w[Port], %b[byte]"
            : [byte] "=a"(byte)
    : [Port] "Nd"(Port));
    return byte;
}

static __inline__ __attribute__((always_inline, artificial)) unsigned short __inword(
        const unsigned short Port) {
    unsigned short word;
    __asm__ __volatile__("inw %w[Port], %w[word]"
            : [word] "=a"(word)
    : [Port] "Nd"(Port));
    return word;
}

static __inline__ __attribute__((always_inline, artificial)) unsigned long __indword(
        const unsigned short Port) {
    unsigned long dword;
    __asm__ __volatile__("inl %w[Port], %k[dword]"
            : [dword] "=a"(dword)
    : [Port] "Nd"(Port));
    return dword;
}

static __inline__ __attribute__((always_inline, artificial)) void __inbytestring(
        unsigned short Port, unsigned char *Buffer, unsigned long Count) {
    __asm__ __volatile__("rep; insb"
            : [Buffer] "=D"(Buffer), [Count] "=c"(Count)
    : "d"(Port), "[Buffer]"(Buffer), "[Count]"(Count)
    : "memory");
}

static __inline__ __attribute__((always_inline, artificial)) void __inwordstring(
        unsigned short Port, unsigned short *Buffer, unsigned long Count) {
    __asm__ __volatile__("rep; insw"
            : [Buffer] "=D"(Buffer), [Count] "=c"(Count)
    : "d"(Port), "[Buffer]"(Buffer), "[Count]"(Count)
    : "memory");
}

static __inline__ __attribute__((always_inline, artificial)) void __indwordstring(
        unsigned short Port, unsigned long *Buffer, unsigned long Count) {
    __asm__ __volatile__("rep; insl"
            : [Buffer] "=D"(Buffer), [Count] "=c"(Count)
    : "d"(Port), "[Buffer]"(Buffer), "[Count]"(Count)
    : "memory");
}

static __inline__ __attribute__((always_inline, artificial)) void __outbyte(
        unsigned short const Port, const unsigned char Data) {
    __asm__ __volatile__("outb %b[Data], %w[Port]"
            :
            : [Port] "Nd"(Port), [Data] "a"(Data));
}

static __inline__ __attribute__((always_inline, artificial)) void __outword(
        unsigned short const Port, const unsigned short Data) {
    __asm__ __volatile__("outw %w[Data], %w[Port]"
            :
            : [Port] "Nd"(Port), [Data] "a"(Data));
}

static __inline__ __attribute__((always_inline, artificial)) void __outdword(
        unsigned short const Port, const unsigned long Data) {
    __asm__ __volatile__("outl %k[Data], %w[Port]"
            :
            : [Port] "Nd"(Port), [Data] "a"(Data));
}

static __inline__ __attribute__((always_inline, artificial)) void __outbytestring(
        unsigned short const Port, const unsigned char *const Buffer,
        const unsigned long Count) {
    __asm__ __volatile__("rep; outsb"
            :
            : [Port] "d"(Port), [Buffer] "S"(Buffer), "c"(Count));
}

static __inline__ __attribute__((always_inline, artificial)) void __outwordstring(
        unsigned short const Port, const unsigned short *const Buffer,
        const unsigned long Count) {
    __asm__ __volatile__("rep; outsw"
            :
            : [Port] "d"(Port), [Buffer] "S"(Buffer), "c"(Count));
}

static __inline__ __attribute__((always_inline, artificial)) void __outdwordstring(
        unsigned short const Port, const unsigned long *const Buffer,
        const unsigned long Count) {
    __asm__ __volatile__("rep; outsl"
            :
            : [Port] "d"(Port), [Buffer] "S"(Buffer), "c"(Count));
}
static __inline__ __attribute__((always_inline, artificial)) unsigned long __readcr0(void) {
    unsigned long value;
    __asm__ __volatile__("mov %%cr0, %[value]" : [value] "=q"(value));
    return value;
}

static __inline__ __attribute__((always_inline, artificial)) unsigned long __readcr2(void) {
    unsigned long value;
    __asm__ __volatile__("mov %%cr2, %[value]" : [value] "=q"(value));
    return value;
}

static __inline__ __attribute__((always_inline, artificial)) unsigned long __readcr3(void) {
    unsigned long value;
    __asm__ __volatile__("mov %%cr3, %[value]" : [value] "=q"(value));
    return value;
}

static __inline__ __attribute__((always_inline, artificial)) unsigned long __readcr4(void) {
    unsigned long value;
    __asm__ __volatile__("mov %%cr4, %[value]" : [value] "=q"(value));
    return value;
}

static __inline__ __attribute__((always_inline, artificial, target("general-regs-only"))) unsigned long __readcr8(void) {
    unsigned long value;
    __asm__ __volatile__("mov %%cr8, %[value]" : [value] "=q"(value));
    return value;
}

static __inline__ __attribute__((always_inline, artificial)) void __writecr0(
        const unsigned long long Data) {
    __asm__("mov %[Data], %%cr0"
            :
            : [Data] "q"((const unsigned long)(Data & 0xFFFFFFFF))
    : "memory");
}

static __inline__ __attribute__((always_inline, artificial)) void __writecr3(
        const unsigned long long Data) {
    __asm__("mov %[Data], %%cr3"
            :
            : [Data] "q"((const unsigned long)(Data & 0xFFFFFFFF))
    : "memory");
}

static __inline__ __attribute__((always_inline, artificial)) void __writecr4(
        const unsigned long long Data) {
    __asm__("mov %[Data], %%cr4"
            :
            : [Data] "q"((const unsigned long)(Data & 0xFFFFFFFF))
    : "memory");
}

static __inline__ __attribute__((always_inline, artificial)) void __writecr8(
        const unsigned long long Data) {
    __asm__("mov %[Data], %%cr8"
            :
            : [Data] "q"((const unsigned long)(Data & 0xFFFFFFFF))
    : "memory");
}


static __inline__ __attribute__((always_inline, artificial)) void __invlpg(
        void *const Address) {
    __asm__("invlpg %[Address]" : : [Address] "m"(*((unsigned char *)(Address))));
}

static __inline__ __attribute__((always_inline, artificial)) unsigned long long __readmsr(
        const int reg) {
    uint32_t low;
    uint32_t high;
    __asm__ __volatile__("rdmsr" : "=d"(high), "=a"(low) : "c"(reg));
    return (uint64_t)high << 32 | low;
}

extern __inline void __attribute__((__gnu_inline__, __always_inline__, __artificial__, target("general-regs-only"))) __writemsr(
        const unsigned long Register, const unsigned long long Value) {
    __asm__ __volatile__("wrmsr" : : "d"((uint32_t)(Value >> 32)), "a"((uint32_t)Value), "c"(Register));
}

static __inline__ __attribute__((always_inline, artificial)) void __cpuid(
        int CPUInfo[], const int InfoType) {
    __asm__ __volatile__("cpuid"
            : "=a"(CPUInfo[0]), "=b"(CPUInfo[1]), "=c"(CPUInfo[2]),
    "=d"(CPUInfo[3])
            : "a"(InfoType));
}

static __inline__ void __DEFAULT_FN_ATTRS __halt(void) {
    __asm__ volatile("hlt");
}

static __inline__ void __DEFAULT_FN_ATTRS __nop(void) {
    __asm__ volatile("nop");
}

#ifdef __cplusplus
}
#endif

#undef __LPTRINT_TYPE__

#undef __DEFAULT_FN_ATTRS

#endif /* __INTRIN_H */
