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

#include <arch/intrin.h>
#include <util/except.h>
#include <util/string.h>

#include "scheduler.h"
#include "time/timer.h"
#include "arch/apic.h"
#include "mem/stack.h"

#include <stdatomic.h>


static bool acquire_preemption() {
    bool ints = (__readeflags() & BIT9) ? true : false;
    _disable();

}

static void release_preemption(bool ints) {
    if (ints) {
        _enable();
    }
}


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
    bool ints = acquire_preemption();

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

    release_preemption(ints);

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

void save_thread_context(thread_t* restrict target, interrupt_context_t* restrict ctx) {
    thread_registers_t* regs = &target->regs;
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

void restore_thread_context(thread_t* restrict target, interrupt_context_t* restrict ctx) {
    thread_registers_t* regs = &target->regs;
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
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TLS initialization
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * The TLS size
 */
static size_t m_tls_size = 0;

err_t init_tls() {
    err_t err = NO_ERROR;



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

// the global array of free thread descriptors
static spinlock_t m_global_free_threads_lock = INIT_SPINLOCK();
static thread_list_t m_global_free_threads;
static int32_t m_global_free_threads_count = 0;

// cpu local free threads
static CPU_LOCAL thread_list_t m_free_threads;
static CPU_LOCAL int32_t m_free_threads_count = 0;

static thread_t* get_free_thread() {
    thread_list_t* free_threads = get_cpu_local_base(&m_free_threads);

    preempt_state_t preempt = scheduler_preempt_disable();

retry:
    // If we have no threads and there are threads in the global free list pull
    // some threads to us, only take up to 32 entries
    if (thread_list_empty(free_threads) &&!thread_list_empty(&m_global_free_threads)) {
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
    memset((void*)(thread->tcb - m_tls_size), 0, m_tls_size);

cleanup:
    scheduler_preempt_enable(preempt);
    return thread;
}

static thread_t* alloc_thread() {
    err_t err = NO_ERROR;

    thread_t* thread = malloc(sizeof(thread_t));
    CHECK(thread != NULL);

    // allocate the tcb
    thread->tcb = (uintptr_t) malloc(m_tls_size + sizeof(void*));
    CHECK(thread->tcb != 0);
    thread->tcb += m_tls_size;

    // set the tcb base in the tcb (part of sysv)
    *(uintptr_t*)thread->tcb = thread->tcb;

cleanup:
    if (IS_ERROR(err)) {
        if (thread != NULL) {
            if (thread->tcb != 0) {
                free((void*)(thread->tcb - m_tls_size));
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
    }

    thread->regs.rip = (uint64_t) entry;
    thread->regs.rflags = BIT1 | BIT9 | BIT21; // Always 1 bit, Interrupt enable flag, Able to use CPUID instruction
    thread->regs.rsp = (uint64_t)alloc_stack();

    // we want the return address to be thread_exit
    // and the stack to be aligned to 16 bytes + 8
    // as per the sys-v abi (http://www.x86-64.org/documentation/abi.pdf)
    PUSH64(thread->regs.rsp, 0);
    PUSH64(thread->regs.rsp, 0);
    PUSH64(thread->regs.rsp, thread_exit);

    // set the state as waiting
    cas_thread_state(thread, THREAD_STATUS_IDLE, THREAD_STATUS_WAITING);

    return thread;
}

void thread_exit() {
    // simply signal the scheduler to drop the current thread, it will
    // release the thread properly on its on
    scheduler_drop_current();
}

