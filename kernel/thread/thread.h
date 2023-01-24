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

typedef struct thread {
    // the thread name, keep at zero so when
    // printing a name of a null thread it will
    // NULL
    char name[64];

    // unique id for the thread
    uint16_t id;

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
    // Related to parking lot
    //

    // the sync block we are waiting on
    const void* address;

    // unpark token
    intptr_t token;

    // lock used for synchronizing the parking
    spinlock_t parking_lock;

    // used for the wait queue in parking lot
    struct thread* next_in_queue;

    //
    // Other
    //

    // mimalloc heap
    void* heap;
    // microtime() of the start of the current run and sleep
    // this accounts from when the tasl is marked runnable to when it starts sleeping
    // (so that excludes the RUNNABLE->RUNNING) delay
    // and likewise for the sleep, it only counts the voluntary sleep time (timers, waitables, semaphore/mutex)
    // and not the time when the task was not running due to preemption  
    
    
    void	*ts_runq;	/* Run-queue we're queued on. */
	short		ts_flags;	/* TSF_* flags. */
	int		ts_cpu;		/* CPU that we have affinity for. */
	int		ts_rltick;	/* Real last tick, for affinity. */
	int		ts_slice;	/* Ticks of slice remaining. */
	unsigned int		ts_slptime;	/* Number of ticks we vol. slept */
	unsigned 		ts_runtime;	/* Number of ticks we were running */
	int		ts_ltick;	/* Last tick that we were running on */
	int		ts_ftick;	/* First tick that we were running on */
	int		ts_ticks;	/* Tick count */

    
    // the time spent where the process was runnable and in voluntary sleep
    // only the last 5 seconds are kept, look at SCHEDULER_SLIDING_WINDOW_MS for an explanation
    int runtime;
    int incruntime;
    int pri_class;
    int base_ithread_pri;
    int slice;
    int base_pri;
    // idk
    int ticks;
    int ltick;
    int ftick;
    int rltick;
    int slptick;
    int critnest;
    int swvoltick;
    int swinvoltick;
    uint32_t inhibitors;
    int lastcpu;
    int oncpu;
    uint8_t priority;
    int spinlock_count;
    uint32_t flags;
    int rqindex;
    uint32_t state;
    //int cpu;
    int user_pri;
    int base_user_pri;
    int lend_user_pri;
    
    int owepreempt;
    bool spinlock_status;
    int sched_ast;

    spinlock_t* lock; // this points to a threadqueue lock
    // intrusive linked list of threads in the same priority bucket
    struct thread* next_in_bucket; 
    struct thread** prev_in_bucket;
} thread_t;

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

/**
 * A counter for how many threads we have
 */
extern atomic_int g_thread_count;

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
