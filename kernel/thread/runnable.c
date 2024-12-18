#include "runnable.h"

#include <stdbool.h>
#include <arch/intrin.h>
#include <lib/defs.h>

void runnable_set_rsp(runnable_t* to, void* rsp) {
    to->rsp = rsp - 8 * 3;
}

void runnable_set_rip(runnable_t* to, void* rip) {
    *((void**)to->rsp) = rip;
}

__attribute__((naked))
static void runnable_switch_internal(runnable_t* from, runnable_t* to) {
    asm(
        "movq %%rsp, %P0(%%rdi)\n"
        "movq %P0(%%rsi), %%rsp\n"
        "ret"
        :
        : "i"(offsetof(runnable_t, rsp))
        : "memory"
    );
}

STATIC_ASSERT(sizeof(atomic_flag) == 1);

void runnable_switch(runnable_t* from, runnable_t* to) {
    asm(
        "call %P0"
        :
        : "i"(runnable_switch_internal), "D"(from), "S"(to)
        : "rax", "rbx", "rcx", "rdx", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "memory"
    );
}

__attribute__((naked))
void runnable_jump(runnable_t* to) {
    asm(
        "movq %P0(%%rdi), %%rsp\n"
        "ret"
        :
        : "i"(offsetof(runnable_t, rsp))
        : "memory"
    );
}
