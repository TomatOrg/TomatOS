/*
 * Code taken and modified from Go
 *
 * Copyright (c) 2009 The Go Authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *    * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "thread.h"

#include "cpu_local.h"

#include <util/except.h>
#include <util/string.h>
#include <util/stb_ds.h>

#include <sync/word_lock.h>

#include <time/tsc.h>

#include <arch/apic.h>
#include <arch/msr.h>

#include <mem/stack.h>

#include "scheduler.h"
#include "kernel.h"

#include <util/elf64.h>

#include <stdatomic.h>
#include "arch/intrin.h"
#include "dotnet/jit/jit.h"
#include "arch/gdt.h"
#include "sync/parking_lot.h"

// TODO: remove the conversion and use a single format
thread_status_t get_thread_status(thread_t* thread) {
    unsigned int state = thread->state;
    if (state == TDS_RUNNING) return THREAD_STATUS_RUNNING;
    if (state == TDS_RUNQ || state == TDS_CAN_RUN) return THREAD_STATUS_RUNNABLE;
    if (state == TDS_INHIBITED) return THREAD_STATUS_WAITING;
    if (state == TDS_INACTIVE) return THREAD_STATUS_IDLE;
    __builtin_unreachable();
}

static void save_fx_state(thread_fx_save_state_t* state) {
    _fxsave64(state);
}

INTERRUPT void save_thread_context(thread_t* restrict target, interrupt_context_t* restrict ctx) {
    thread_save_state_t* regs = &target->save_state;
    save_fx_state(&regs->fx_save_state);
    regs->r15 = ctx->r15;
    regs->r14 = ctx->r14;
    regs->r13 = ctx->r13;
    regs->r12 = ctx->r12;
    regs->r11 = ctx->r11;
    regs->r10 = ctx->r10;
    regs->r9 = ctx->r9;
    regs->r8 = ctx->r8;
    regs->rbp = ctx->rbp;
    regs->rdi = ctx->rdi;
    regs->rsi = ctx->rsi;
    regs->rdx = ctx->rdx;
    regs->rcx = ctx->rcx;
    regs->rbx = ctx->rbx;
    regs->rax = ctx->rax;
    regs->rip = ctx->rip;
    regs->rflags = ctx->rflags;
    regs->rsp = ctx->rsp;
}

static void restore_fx_state(thread_fx_save_state_t* state) {
    _fxrstor64(state);
}

INTERRUPT void restore_thread_context(thread_t* restrict target, interrupt_context_t* restrict ctx) {
    thread_save_state_t* regs = &target->save_state;
    ctx->r15 = regs->r15;
    ctx->r14 = regs->r14;
    ctx->r13 = regs->r13;
    ctx->r12 = regs->r12;
    ctx->r11 = regs->r11;
    ctx->r10 = regs->r10;
    ctx->r9 = regs->r9;
    ctx->r8 = regs->r8;
    ctx->rbp = regs->rbp;
    ctx->rdi = regs->rdi;
    ctx->rsi = regs->rsi;
    ctx->rdx = regs->rdx;
    ctx->rcx = regs->rcx;
    ctx->rbx = regs->rbx;
    ctx->rax = regs->rax;
    ctx->rip = regs->rip;
    ctx->rflags = regs->rflags;
    ctx->rsp = regs->rsp;
    ctx->cs = GDT_CODE;
    ctx->ss = GDT_DATA;
    restore_fx_state(&regs->fx_save_state);
    __writemsr(MSR_IA32_FS_BASE, (uintptr_t)target->tcb);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TLS initialization
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * The TLS size
 */
static size_t m_tls_size = 0;

/**
 * The TLS alignement
 */
static size_t m_tls_align = 0;

/*
*/
static size_t m_tls_filesz = 0;
static uint8_t* m_tls_file = NULL;

err_t init_tls() {
    void* kernel = g_limine_kernel_file.response->kernel_file->address;

    err_t err = NO_ERROR;
    Elf64_Ehdr* ehdr = kernel;

    // Find the stuff we need
    CHECK(ehdr->e_phoff != 0);
    Elf64_Phdr* segments = kernel + ehdr->e_phoff;
    for (int i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr* segment = &segments[i];
        if (segment->p_type == PT_TLS) {
            if (segment->p_filesz) {
                m_tls_filesz = segment->p_filesz;
                m_tls_file = malloc(m_tls_filesz);
                memcpy(m_tls_file, kernel + segment->p_offset, m_tls_filesz);
            }
            // take the tls size and properly align it
            m_tls_size = segment->p_memsz;
            m_tls_size += (-m_tls_size - segment->p_vaddr) & (segment->p_align - 1);
            m_tls_align = segment->p_align;

            TRACE("tls: memsz=%d filesz=%d", m_tls_size, m_tls_filesz);
            break;
        }
    }

cleanup:
    return err;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Thread creation and deletion
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct thread_list {
    thread_t* head;
} thread_list_t;

static bool thread_list_empty(thread_list_t* list) {
    return list->head == NULL;
}

static void thread_list_push(thread_list_t* list, thread_t* thread) {
    thread->sched_link = list->head;
    list->head = thread;
}

static thread_t* thread_list_pop(thread_list_t* list) {
    thread_t* thread = list->head;
    if (thread != NULL) {
        list->head = thread->sched_link;
    }
    return thread;
}

// all the threads in the system
static word_lock_t m_all_threads_lock = {0 };
thread_t** g_all_threads = NULL;

static void add_to_all_threads(thread_t* thread) {
    lock_all_threads();
    // set the default gc thread data, updated by the gc whenever it iterates the
    // thread list and does stuff
    thread->tcb->gc_data = m_default_gc_thread_data;
    arrpush(g_all_threads, thread);
    unlock_all_threads();
}

// the global array of free thread descriptors
static spinlock_t m_global_free_threads_lock = INIT_SPINLOCK();
static thread_list_t m_global_free_threads;
static int32_t m_global_free_threads_count = 0;

// cpu local free threads
static CPU_LOCAL thread_list_t m_free_threads;
static CPU_LOCAL int32_t m_free_threads_count = 0;

static thread_t* get_free_thread() {
    thread_list_t* free_threads = get_cpu_local_base(&m_free_threads);

    scheduler_preempt_disable();

retry:
    // If we have no threads and there are threads in the global free list pull
    // some threads to us, only take up to 32 entries
    if (thread_list_empty(free_threads) && !thread_list_empty(&m_global_free_threads)) {
        // We got no threads on our cpu, move a batch to our cpu
        spinlock_lock(&m_global_free_threads_lock);
        while (m_free_threads_count < 32) {
            // prefer threads with stacks
            thread_t* thread = thread_list_pop(&m_global_free_threads);
            if (thread == NULL) {
                break;
            }
            m_global_free_threads_count--;
            thread_list_push(free_threads, thread);
            m_free_threads_count++;
        }
        spinlock_unlock(&m_global_free_threads_lock);
        goto retry;
    }

    // take a thread from the local list
    thread_t* thread = thread_list_pop(free_threads);
    if (thread == NULL) {
        goto cleanup;
    }
    m_free_threads_count--;
    
cleanup:
    scheduler_preempt_enable();
    return thread;
}

/**
 * Used to generate new thread ids
 */
static atomic_int m_thread_id_gen = 1;

static thread_t* alloc_thread() {
    err_t err = NO_ERROR;

    int thread_id = atomic_fetch_add(&m_thread_id_gen, 1);
    CHECK(thread_id <= UINT16_MAX);

    thread_t* thread = malloc(sizeof(thread_t));
    CHECK(thread != NULL);

    // set the id
    thread->id = thread_id;

    // allocate a new stack
    thread->stack_top = alloc_stack();
    CHECK(thread->stack_top != NULL);

    // allocate the tcb
    void* tcb_bottom = malloc_aligned(m_tls_size + sizeof(thread_control_block_t), m_tls_align);
    CHECK(tcb_bottom != NULL);
    thread->tcb = tcb_bottom + m_tls_size;

    // set the tcb base in the tcb (part of sysv)
    thread->tcb->tcb = thread->tcb;

cleanup:
    if (IS_ERROR(err)) {
        if (thread != NULL) {
            if (thread->tcb != 0) {
                free((void*)thread->tcb - m_tls_size);
            }
            free(thread);
            thread = NULL;
        }
    }

    return thread;
}

atomic_int g_thread_count = 0;

thread_t* create_thread(thread_entry_t entry, void* ctx, const char* fmt, ...) {
    thread_t* thread = get_free_thread();
    if (thread == NULL) {
        thread = alloc_thread();
        if (thread == NULL) {
            return NULL;
        }
        add_to_all_threads(thread);
    }

    // increment the thread count and let
    // parking lot know it happened
    int thread_count = atomic_fetch_add(&g_thread_count, 1) + 1;
    parking_lot_rehash(thread_count);

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(thread->name, sizeof(thread->name), fmt, ap);
    va_end(ap);

    // thread starts with a single reference that is considered to belong to the scheduler
    // that means that the caller should not actually release the thread on its own, but only
    // if he plans to continue using it after the thread_ready
    thread->ref_count = 1;

    // clean up thread-local data

    // clear the thread control block for the new thread
    // we need to re-do it even if the thread was obtained from the freelist
    // as the TLS data might've changed
    // NOTE: the TLS data is stored at negative offsets from FS_BASE (set to thread->tcb)
    // and at positive offset there is the thread_control_block_t structure
    // super important, the tcb itself must not be cleared, the gc
    // relies on the values to be consistent!
    uint8_t* tcb_bottom = (uint8_t*)thread->tcb - m_tls_size;
    memset(tcb_bottom, 0, m_tls_size);
    if (m_tls_filesz) memcpy(tcb_bottom, m_tls_file, m_tls_filesz); // make ubsan happy about non-null pointer

    // Reset the thread save state:
    //  - set the rip as the thread entry
    //  - set the rflags for ALWAYS_1 | IF
    memset(&thread->save_state, 0, sizeof(thread->save_state));
    thread->save_state.rip = (uint64_t) entry;
    thread->save_state.rflags = (rflags_t){ .always_one = 1, .IF = 1 };
    thread->save_state.rsp = (uint64_t) thread->stack_top;

    // set the context
    thread->save_state.rdi = (uintptr_t)ctx;

    // we want the return address to be thread_exit
    // and the stack to be aligned to 16 bytes + 8
    // as per the sys-v abi (http://www.x86-64.org/documentation/abi.pdf)
    PUSH64(thread->save_state.rsp, 0);
    PUSH64(thread->save_state.rsp, 0);
    PUSH64(thread->save_state.rsp, thread_exit);

    // finally setup a proper floating point context (according to sys-v abi)
    thread->save_state.fx_save_state.fcw = BIT0 | BIT1 | BIT2 | BIT3 | BIT4 | BIT5 | BIT8 | BIT9;
    thread->save_state.fx_save_state.mxcsr = 0b1111110000000;

    sched_new_thread(thread);

    return thread;
}

void lock_all_threads() {
    word_lock_lock(&m_all_threads_lock);
}

void unlock_all_threads() {
    word_lock_unlock(&m_all_threads_lock);
}

void thread_exit() {
    // simply signal the scheduler to drop the current thread, it will
    // release the thread properly on its on
    scheduler_drop_current();
    __builtin_unreachable();
}

thread_t* put_thread(thread_t* thread) {
    atomic_fetch_add(&thread->ref_count, 1);
    return thread;
}

void release_thread(thread_t* thread) {
    if (atomic_fetch_sub(&thread->ref_count, 1) == 1) {
        thread_list_t* free_threads = get_cpu_local_base(&m_free_threads);

        // free the thread locals of this thread as we don't need them anymore
        jit_free_thread_locals();

        // add to the list
        thread_list_push(free_threads, thread);
        m_free_threads_count++;

        // if we have too many threads locally on the cpu
        // move some of them to the global threads list
        if (m_free_threads_count >= 64) {
            spinlock_lock(&m_global_free_threads_lock);
            while (m_free_threads_count >= 32) {
                thread = thread_list_pop(free_threads);
                m_free_threads_count--;
                thread_list_push(&m_global_free_threads, thread);
                m_global_free_threads_count++;
            }
            spinlock_unlock(&m_global_free_threads_lock);
        }
    }
}

void reclaim_free_threads() {
    int free_count = 0;

    spinlock_lock(&m_global_free_threads_lock);
    while (!thread_list_empty(&m_global_free_threads)) {
        thread_t* thread = thread_list_pop(&m_global_free_threads);
        m_global_free_threads_count--;

        // free the thread control block
        void* tcb = thread->tcb - m_tls_size;
        free(tcb);

        // free the stack
        free_stack(thread->stack_top);

        // free the thread itself
        free(thread);
    }
    spinlock_unlock(&m_global_free_threads_lock);

    TRACE("Reclaimed %d threads from the global free list", free_count);
}
