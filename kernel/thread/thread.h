#pragma once

#include <dotnet/gc/gc_thread_data.h>

#include <sync/spinlock.h>
#include <util/except.h>
#include <arch/idt.h>

#include <stdalign.h>

typedef enum thread_status {
    /**
     * Means this thread was just allocated and has not
     * yet been initialized
     */
    THREAD_STATUS_IDLE = 0,

    /**
     * Means this thread is on a run queue. It is
     * not currently executing user code.
     */
    THREAD_STATUS_RUNNABLE = 1,

    /**
     * Means this thread may execute user code.
     */
    THREAD_STATUS_RUNNING = 2,

    /**
     * Means this thread is blocked in the runtime.
     * It is not executing user code. It is not on a run queue,
     * but should be recorded somewhere so it can be scheduled
     * when necessary.
     */
    THREAD_STATUS_WAITING = 3,

    /**
     * Means the thread stopped itself for a suspend
     * preemption. IT is like THREAD_STATUS_WAITING, but
     * nothing is yet responsible for readying it. some
     * suspend must CAS the status to THREAD_STATUS_WAITING
     * to take responsibility for readying this thread
     */
    THREAD_STATUS_PREEMPTED = 4,

    /**
     * Means this thread is currently unused. It may be
     * just exited, on a free list, or just being initialized.
     * It is not executing user code.
     */
    THREAD_STATUS_DEAD = 5,

    /**
     * Indicates someone wants to suspend this thread (probably the
     * garbage collector).
     */
    THREAD_SUSPEND = 0x1000,
} thread_status_t;

typedef struct thread_fx_save_state {
    uint16_t fcw;
    uint16_t fsw;
    uint8_t ftw;
    uint8_t _reserved0;
    uint16_t opcode;
    uint64_t fip;
    uint64_t fdp;
    uint32_t mxcsr;
    uint32_t mxcsr_mask;
    uint8_t st0mm0[10];
    uint8_t _reserved3[6];
    uint8_t st1mm1[10];
    uint8_t _reserved4[6];
    uint8_t st2mm2[10];
    uint8_t _reserved5[6];
    uint8_t st3mm3[10];
    uint8_t _reserved6[6];
    uint8_t st4mm4[10];
    uint8_t _reserved7[6];
    uint8_t st5mm5[10];
    uint8_t _reserved8[6];
    uint8_t st6mm6[10];
    uint8_t _reserved9[6];
    uint8_t st7mm7[10];
    uint8_t _reserved10[6];
    uint8_t xmm0[16];
    uint8_t xmm1[16];
    uint8_t xmm2[16];
    uint8_t xmm3[16];
    uint8_t xmm4[16];
    uint8_t xmm5[16];
    uint8_t xmm6[16];
    uint8_t xmm7[16];
    uint8_t xmm8[16];
    uint8_t xmm9[16];
    uint8_t xmm10[16];
    uint8_t xmm11[16];
    uint8_t xmm12[16];
    uint8_t xmm13[16];
    uint8_t xmm14[16];
    uint8_t xmm15[16];
    uint8_t _reserved11[3 * 16];
    uint8_t available[3 * 16];
} PACKED thread_fx_save_state_t;
STATIC_ASSERT(sizeof(thread_fx_save_state_t) == 512);

typedef struct thread_save_state {
    // fpu/sse/sse2
    alignas(16) thread_fx_save_state_t fx_save_state;

    // gprs
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t rip;
    rflags_t rflags;
    uint64_t rsp;
} thread_save_state_t;

typedef struct thread_control_block {
    struct thread_control_block* tcb;

    // The per-thread data for the gc
    gc_thread_data_t gc_data;

    // the managed thread instance for this thread
    void* managed_thread;
} thread_control_block_t;

typedef struct waiting_thread waiting_thread_t;

typedef struct thread {
    // the thread name
    char name[64];

    // ref count
    atomic_size_t ref_count;

    //
    // The thread context
    //

    // gprs
    thread_save_state_t save_state;

    // thread control block
    thread_control_block_t* tcb;

    // the top of the stack, so
    // we can free it later
    void* stack_top;

    //
    // scheduling related
    //

    // transition to THREAD_STATUS_PREEMPTED on preemption, otherwise just deschedule
    bool preempt_stop;

    // The current status of the thread
    _Atomic(thread_status_t) status;

    // Link for the scheduler
    struct thread* sched_link;

    //
    // Waitable
    //

    // the waiting thread structure that caused the thread to wakeup
    waiting_thread_t* waker;

    // list of waiting threads structures that point to this thread
    waiting_thread_t* waiting;

    // are we participating in a select and did someone win the race?
    _Atomic(uint32_t) select_done;

    // mimalloc heap
    void* heap;
} thread_t;

struct waitable;

struct waiting_thread {
    thread_t* thread;

    // only used in the cache
    waiting_thread_t* next;
    waiting_thread_t* prev;

    uint32_t ticket;

    waiting_thread_t* wait_link;
    waiting_thread_t* wait_tail;

    bool is_select;
    bool success;
    struct waitable* waitable;
};

/**
 * For thread-locals
 */
#define THREAD_LOCAL _Thread_local

///**
// * The default thread control block, this is what we set when
// * a new thread is created, must be called under the thread lock
// */
//extern thread_control_block_t m_default_tcb;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Waiting thread item
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Acquire a new waiting thread descriptor
 */
waiting_thread_t* acquire_waiting_thread();

/**
 * Release a waiting thread descriptor
 *
 * @param wt    [IN] The WT descriptor
 */
void release_waiting_thread(waiting_thread_t* wt);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Thread creation and destruction
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef void(*thread_entry_t)(void* ctx);

/**
 * Create a new thread
 */
thread_t* create_thread(thread_entry_t entry, void* ctx, const char* fmt, ...);

/**
 * Exits from the currently running thread
 */
void thread_exit();

/**
 * Increases the ref count of a thread
 */
thread_t* put_thread(thread_t* thread);

/**
 * Free a thread
 *
 * Must be called from a context with no preemption
 */
void release_thread(thread_t* thread);

/**
 * Reclaims free threads from the global free list, useful
 * if the kernel heap has run out of memory or even if we need
 * more free pages (as it will free stacks that can be reclaimed
 * as well)
 */
void reclaim_free_threads();

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// All thread iteration
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * All the threads in the system are part of this array
 */
extern thread_t** g_all_threads;

void lock_all_threads();

void unlock_all_threads();

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Thread status
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Get the status of a thread atomically
 *
 * @param thread    [IN] The target thread
 */
thread_status_t get_thread_status(thread_t* thread);

/**
 * Compare and swap the thread state atomically
 *
 * @remark
 * This will suspend until the thread status is equals to old and only then try to
 * set it to new, if that fails it will continue to try until it has a success.
 *
 * @param thread    [IN] The target thread
 * @param old       [IN] The old status
 * @param new       [IN] The new status
 */
void cas_thread_state(thread_t* thread, thread_status_t old, thread_status_t new);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Thread context
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void save_thread_context(thread_t* restrict target, interrupt_context_t* restrict ctx);

void restore_thread_context(thread_t* restrict target, interrupt_context_t* restrict ctx);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Thread local stuff
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Initialize TLS for the kernel, must be called before
 * threads are created
 */
err_t init_tls();
