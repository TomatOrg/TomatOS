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

#include <sync/mutex.h>

#include <time/timer.h>

#include <arch/apic.h>
#include <arch/msr.h>

#include <mem/stack.h>

#include "scheduler.h"
#include "kernel.h"

#include <util/elf64.h>

#include <stdatomic.h>
#include "arch/intrin.h"


//
// Global waiting thread cache
//
static spinlock_t m_global_wt_lock;
static waiting_thread_t* m_global_wt_cache;

//
// Per-cpu waiting thread cache
//
static waiting_thread_t* CPU_LOCAL m_wt_cache[128];
static uint8_t CPU_LOCAL m_wt_cache_len = 0;

waiting_thread_t* acquire_waiting_thread() {
    // we disable interrupts in here so we can do stuff
    // atomically on the current core
    scheduler_preempt_disable();

    // check if we have any local cached
    if (m_wt_cache_len == 0) {

        // try to get a bunch from the central cache, maximum to half the size
        // of the local cache
        spinlock_lock(&m_global_wt_lock);
        while ((m_wt_cache_len < ARRAY_LEN(m_wt_cache) / 2) && m_global_wt_cache != NULL) {
            waiting_thread_t* wt = m_global_wt_cache;
            m_global_wt_cache = wt->next;
            wt->next = NULL;
            m_wt_cache[m_wt_cache_len++] = wt;
        }
        spinlock_unlock(&m_global_wt_lock);

        if (m_wt_cache_len == 0) {
            // central cache is empty, allocate a new one
            m_wt_cache[m_wt_cache_len++] = malloc(sizeof(waiting_thread_t));
        }
    }

    // pop one
    waiting_thread_t* wt = m_wt_cache[--m_wt_cache_len];
    m_wt_cache[m_wt_cache_len] = NULL;

    scheduler_preempt_enable();

    return wt;
}

void release_waiting_thread(waiting_thread_t* wt) {
    // we disable interrupts in here so we can do stuff
    // atomically on the current core
    bool ints = (__readeflags() & BIT9) ? true : false;
    _disable();

    if (m_wt_cache_len == ARRAY_LEN(m_wt_cache)) {
        // Transfer half of the local cache to the central cache
        waiting_thread_t* first = NULL;
        waiting_thread_t* last = NULL;

        while (m_wt_cache_len > ARRAY_LEN(m_wt_cache) / 2) {
            waiting_thread_t* p = m_wt_cache[--m_wt_cache_len];
            m_wt_cache[m_wt_cache_len] = NULL;
            if (first == NULL) {
                first = p;
            } else {
                last->next = p;
            }
            last = p;
        }

        spinlock_lock(&m_global_wt_lock);
        last->next = m_global_wt_cache;
        m_global_wt_cache = last;
        spinlock_unlock(&m_global_wt_lock);
    }

    // put into the local cache
    m_wt_cache[m_wt_cache_len++] = wt;

    if (ints) {
        _enable();
    }
}

thread_status_t get_thread_status(thread_t* thread) {
    return atomic_load(&thread->status);
}

void cas_thread_state(thread_t* thread, thread_status_t old, thread_status_t new) {
    // sanity
    ASSERT((old & THREAD_SUSPEND) == 0);
    ASSERT((new & THREAD_SUSPEND) == 0);
    ASSERT(old != new);

    // loop if status is in a suspend state giving the GC
    // time to finish and change the state to old val
    thread_status_t old_value = old;
    for (int i = 0; !atomic_compare_exchange_weak(&thread->status, &old_value, new); i++, old_value = old) {
        if (old == THREAD_STATUS_WAITING && thread->status == THREAD_STATUS_RUNNABLE) {
            ASSERT(!"Waiting for THREAD_STATUS_WAITING but is THREAD_STATUS_RUNNABLE");
        }

        // pause for max of 10 times polling for status
        for (int x = 0; x < 10 && thread->status != old; x++) {
            __builtin_ia32_pause();
        }
    }
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
            CHECK(segment->p_filesz == 0); // TODO: preset stuff

            // take the tls size and properly align it
            m_tls_size = segment->p_memsz;
            m_tls_size += (-m_tls_size - segment->p_vaddr) & (segment->p_align - 1);
            m_tls_align = segment->p_align;

            TRACE("tls: %d bytes", m_tls_size);
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
static mutex_t m_all_threads_lock = { 0 };
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

    // clear the TLS area for the new thread
    // super important, the tcb itself must not be cleared, the gc
    // relies on the values to be consistent!
    memset((void*)((uintptr_t)thread->tcb - m_tls_size), 0, m_tls_size);

cleanup:
    scheduler_preempt_enable();
    return thread;
}

static thread_t* alloc_thread() {
    err_t err = NO_ERROR;

    thread_t* thread = malloc(sizeof(thread_t));
    CHECK(thread != NULL);

    thread->stack_bottom = alloc_stack();
    CHECK(thread->stack_bottom != NULL);

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

thread_t* create_thread(thread_entry_t entry, void* ctx, const char* fmt, ...) {
    thread_t* thread = get_free_thread();
    if (thread == NULL) {
        thread = alloc_thread();
        cas_thread_state(thread, THREAD_STATUS_IDLE, THREAD_STATUS_DEAD);
        add_to_all_threads(thread);
    }

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(thread->name, sizeof(thread->name), fmt, ap);
    va_end(ap);

    // Reset the thread save state:
    //  - set the rip as the thread entry
    //  - set the rflags for ALWAYS_1 | IF | ID
    memset(&thread->save_state, 0, sizeof(thread->save_state));
    thread->save_state.rip = (uint64_t) entry;
    thread->save_state.rflags = BIT1 | BIT9 | BIT21;
    thread->save_state.rsp = (uint64_t) thread->stack_bottom;

    // we want the return address to be thread_exit
    // and the stack to be aligned to 16 bytes + 8
    // as per the sys-v abi (http://www.x86-64.org/documentation/abi.pdf)
    PUSH64(thread->save_state.rsp, 0);
    PUSH64(thread->save_state.rsp, 0);
    PUSH64(thread->save_state.rsp, thread_exit);

    // finally setup a proper floating point context (according to sys-v abi)
    thread->save_state.fx_save_state.fcw = BIT0 | BIT1 | BIT2 | BIT3 | BIT4 | BIT5 | BIT8 | BIT9;
    thread->save_state.fx_save_state.mxcsr = BIT7 | BIT8 | BIT9 | BIT10 | BIT11 | BIT12;

    // set the state as waiting
    cas_thread_state(thread, THREAD_STATUS_DEAD, THREAD_STATUS_WAITING);

    return thread;
}

void lock_all_threads() {
    mutex_lock(&m_all_threads_lock);
}

void unlock_all_threads() {
    mutex_unlock(&m_all_threads_lock);
}

void thread_exit() {
    // simply signal the scheduler to drop the current thread, it will
    // release the thread properly on its on
    scheduler_drop_current();
}

void free_thread(thread_t* thread) {
    thread_list_t* free_threads = get_cpu_local_base(&m_free_threads);

    // change the status to dead
    cas_thread_state(thread, THREAD_STATUS_RUNNING, THREAD_STATUS_DEAD);

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
        free_stack(thread->stack_bottom);

        // free the thread itself
        free(thread);
    }
    spinlock_unlock(&m_global_free_threads_lock);

    TRACE("Reclaimed %d threads from the global free list", free_count);
}
