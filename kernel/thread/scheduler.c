/*-
 * Code taken and modified from FreeBSD's ULE scheduler.
 *
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2002-2007, Jeffrey Roberson <jeff@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "scheduler.h"

#include "arch/gdt.h"
#include "arch/idt.h"
#include "cpu_local.h"
#include "irq/irq.h"
#include "mem/mem.h"
#include "mem/stack.h"
#include "sync/irq_spinlock.h"
#include "thread/thread.h"
#include "time/tick.h"
#include "timer.h"
#include "util/defs.h"

#include <arch/apic.h>
#include <arch/intrin.h>
#include <arch/msr.h>
#include <kernel.h>
#include <sync/spinlock.h>
#include <time/tsc.h>
#include <util/fastrand.h>
#include <util/stb_ds.h>

#include <stdatomic.h>

#define MAX_CACHE_LEVELS 2
#define FSHIFT 11 /* bits to right of fixed binary point */
#define FSCALE (1 << FSHIFT)

#define atomic_store_rel_ptr(a, b)                                             \
    atomic_store_explicit((_Atomic(void *) *)a, (void *)b, memory_order_release)
#define atomic_load_int(a) atomic_load((_Atomic(int) *)a)
#define atomic_load_char(a) atomic_load((_Atomic(uint8_t) *)a)
#define atomic_store_char(a, b) atomic_store((_Atomic(uint8_t) *)a, b)

#define NOCPU (-1) /* For when we aren't on a CPU. */

#define SRQ_BORING 0x0000    /* No special circumstances. */
#define SRQ_YIELDING 0x0001  /* We are yielding (from mi_switch). */
#define SRQ_OURSELF 0x0002   /* It is ourself (from mi_switch). */
#define SRQ_INTR 0x0004      /* It is probably urgent. */
#define SRQ_PREEMPTED 0x0008 /* has been preempted.. be kind */
#define SRQ_BORROWING 0x0010 /* Priority updated due to prio_lend */
#define SRQ_HOLD 0x0020      /* Return holding original td lock */
#define SRQ_HOLDTD 0x0040    /* Return holding td lock */

#define RQ_NQS (64) /* Number of run queues. */
#define RQ_PPQ (4)  /* Priorities per queue. */

#define PRIO_MIN -20
#define PRIO_MAX 20

#define PRI_MIN_TIMESHARE (88)
#define PRI_MAX_TIMESHARE (PRI_MIN_IDLE - 1)
#define PRI_MIN_IDLE (224)
#define PRI_MAX (255) /* Lowest priority. */
#define PRI_MAX_IDLE (PRI_MAX)

#define PRI_TIMESHARE_RANGE (PRI_MAX_TIMESHARE - PRI_MIN_TIMESHARE + 1)
#define PRI_INTERACT_RANGE ((PRI_TIMESHARE_RANGE - SCHED_PRI_NRESV) / 2)
#define PRI_BATCH_RANGE (PRI_TIMESHARE_RANGE - PRI_INTERACT_RANGE)

#define PRI_MIN_INTERACT PRI_MIN_TIMESHARE
#define PRI_MAX_INTERACT (PRI_MIN_TIMESHARE + PRI_INTERACT_RANGE - 1)
#define PRI_MIN_BATCH (PRI_MIN_TIMESHARE + PRI_INTERACT_RANGE)
#define PRI_MAX_BATCH PRI_MAX_TIMESHARE
#define PRI_MIN_KERN (48)
#define PRI_MIN_REALTIME (16)
#define PRI_MAX_ITHD (PRI_MIN_REALTIME - 1)

#define PRI_ITHD 1      /* Interrupt thread. */
#define PRI_REALTIME 2  /* Real time process. */
#define PRI_TIMESHARE 3 /* Time sharing process. */
#define PRI_IDLE 4      /* Idle process. */

#define SCHED_PRI_NRESV (PRIO_MAX - PRIO_MIN)
#define SCHED_PRI_NHALF (SCHED_PRI_NRESV / 2)
#define SCHED_PRI_MIN (PRI_MIN_BATCH + SCHED_PRI_NHALF)
#define SCHED_PRI_MAX (PRI_MAX_BATCH - SCHED_PRI_NHALF)
#define SCHED_PRI_RANGE (SCHED_PRI_MAX - SCHED_PRI_MIN + 1)

#define SCHED_INTERACT_MAX (100)
#define SCHED_INTERACT_HALF (SCHED_INTERACT_MAX / 2)
#define SCHED_INTERACT_THRESH (30)
#define SCHED_INTERACT SCHED_INTERACT_THRESH
#define SCHED_PRI_TICKS(ts)                                                    \
    (SCHED_TICK_HZ((ts)) / ((SCHED_TICK_TOTAL((ts)) + SCHED_PRI_RANGE - 1) /   \
                            SCHED_PRI_RANGE)) // ROUNDUP division
#define SCHED_AFFINITY_DEFAULT (MAX(1, hz / 1000))

#define SCHED_TICK_SECS 10
#define SCHED_TICK_TARG (hz * SCHED_TICK_SECS)
#define SCHED_TICK_MAX (SCHED_TICK_TARG + hz)
#define SCHED_TICK_SHIFT 10
#define SCHED_TICK_HZ(ts) ((ts)->ts_ticks >> SCHED_TICK_SHIFT)
#define SCHED_TICK_TOTAL(ts) (MAX((ts)->ts_ltick - (ts)->ts_ftick, hz))
#define SCHED_SLP_RUN_MAX ((hz * 5) << SCHED_TICK_SHIFT)

#define SCHED_AFFINITY(ts, t) ((ts)->ts_rltick > ticks - ((t)*affinity))

#define SCHED_SLICE_DEFAULT_DIVISOR 10 /* ~94 ms, 12 stathz ticks. */
#define SCHED_SLICE_MIN_DIVISOR 6      /* DEFAULT/MIN = ~16 ms. */

#define SW_VOL 0x0100     /* Voluntary switch. */
#define SW_INVOL 0x0200   /* Involuntary switch. */
#define SW_PREEMPT 0x0400 /* The invol switch is a preemption */

#define SWT_NONE 0            /* Unspecified switch. */
#define SWT_PREEMPT 1         /* Switching due to preemption. */
#define SWT_OWEPREEMPT 2      /* Switching due to owepreempt. */
#define SWT_TURNSTILE 3       /* Turnstile contention. */
#define SWT_SLEEPQ 4          /* Sleepq wait. */
#define SWT_SLEEPQTIMO 5      /* Sleepq timeout wait. */
#define SWT_RELINQUISH 6      /* yield call. */
#define SWT_NEEDRESCHED 7     /* NEEDRESCHED was set. */
#define SWT_IDLE 8            /* Switching from the idle thread. */
#define SWT_IWAIT 9           /* Waiting for interrupts. */
#define SWT_SUSPEND 10        /* Thread suspended. */
#define SWT_REMOTEPREEMPT 11  /* Remote processor preempted. */
#define SWT_REMOTEWAKEIDLE 12 /* Remote processor preempted idle. */
#define SWT_COUNT 13          /* Number of switch types. */

#define TSF_BOUND 0x0001    /* Thread can not migrate. */
#define TSF_XFERABLE 0x0002 /* Thread was added as transferable. */

#define TDF_NOLOAD 0x00040000 /* Ignore during load avg calculations. */

#define TDF_BORROWING 0x00000001 /* Thread is borrowing pri from another. */
#define TDF_IDLETD 0x00000020    /* This is a per-CPU idle thread. */
#define TDF_SCHED0 0x01000000    /* Reserved for scheduler private use */
#define TDF_SCHED1 0x02000000    /* Reserved for scheduler private use */
#define TDF_SCHED2 0x04000000    /* Reserved for scheduler private use */
#define TDF_SCHED3 0x08000000    /* Reserved for scheduler private use */

#define TDF_PICKCPU TDF_SCHED0  /* Thread should pick new CPU. */
#define TDF_SLICEEND TDF_SCHED2 /* Thread time slice is over. */

#define TD_GET_STATE(td) td->state
#define TD_SET_STATE(td, val) td->state = (val);
#define TD_IS_RUNNING(td) (TD_GET_STATE(td) == TDS_RUNNING)
#define TD_ON_RUNQ(td) (TD_GET_STATE(td) == TDS_RUNQ)
#define TD_SET_RUNNING(td) TD_SET_STATE(td, TDS_RUNNING)

#define TDI_SUSPENDED 0x0001 /* On suspension queue. */
#define TDI_SLEEPING 0x0002  /* Actually asleep! (tricky). */
#define TDI_SWAPPED 0x0004   /* Stack not in mem.  Bad juju if run. */
#define TDI_LOCK 0x0008      /* Stopped on a lock. */
#define TDI_IWAIT 0x0010     /* Awaiting interrupt. */

#define TD_IS_SLEEPING(td) ((td)->inhibitors & TDI_SLEEPING)
#define TD_ON_SLEEPQ(td) ((td)->wchan != NULL)
#define TD_IS_SUSPENDED(td) ((td)->inhibitors & TDI_SUSPENDED)
#define TD_IS_SWAPPED(td) ((td)->inhibitors & TDI_SWAPPED)
#define TD_ON_LOCK(td) ((td)->inhibitors & TDI_LOCK)
#define TD_AWAITING_INTR(td) ((td)->inhibitors & TDI_IWAIT)
#define TD_IS_RUNNING(td) (TD_GET_STATE(td) == TDS_RUNNING)
#define TD_ON_RUNQ(td) (TD_GET_STATE(td) == TDS_RUNQ)
#define TD_CAN_RUN(td) (TD_GET_STATE(td) == TDS_CAN_RUN)
#define TD_IS_INHIBITED(td) (TD_GET_STATE(td) == TDS_INHIBITED)
#define TD_ON_UPILOCK(td) ((td)->flags & TDF_UPIBLOCKED)
#define TD_IS_IDLETHREAD(td) ((td)->flags & TDF_IDLETD)
#define TD_SET_RUNQ(td) TD_SET_STATE(td, TDS_RUNQ)

#define TD_SET_INHIB(td, inhib)                                                \
    do {                                                                       \
        TD_SET_STATE(td, TDS_INHIBITED);                                       \
        (td)->inhibitors |= (inhib);                                           \
    } while (0)

#define TD_CLR_INHIB(td, inhib)                                                \
    do {                                                                       \
        if (((td)->inhibitors & (inhib)) &&                                    \
            (((td)->inhibitors &= ~(inhib)) == 0))                             \
            TD_SET_STATE(td, TDS_CAN_RUN);                                     \
    } while (0)
#define TD_SET_SLEEPING(td) TD_SET_INHIB((td), TDI_SLEEPING)
#define TD_CLR_SLEEPING(td) TD_CLR_INHIB((td), TDI_SLEEPING)

#define TDQ_SWITCHCNT_INC(tdq)                                                 \
    atomic_fetch_add((_Atomic(int) *)&(tdq)->switchcnt, 1)

#define TD_SET_CAN_RUN(td) TD_SET_STATE(td, TDS_CAN_RUN)

#define thread_lock(tdp)                                                       \
    {                                                                          \
        spinlock_enter();                                                      \
        spinlock_lock((tdp)->lock);                                            \
    }
#define thread_unlock(tdp)                                                     \
    {                                                                          \
        spinlock_unlock((tdp)->lock);                                          \
        spinlock_exit();                                                       \
    }

#define TDQ_LOCKPTR(tdq) (&(tdq)->lock)
#define TDQ_LOCK_ASSERT(rq) ASSERT(rq->lock.locked);
#define TDQ_LOAD(tdq) atomic_load_int(&(tdq)->load)

#define TDQ_LOCK(t)                                                            \
    {                                                                          \
        spinlock_enter();                                                      \
        spinlock_lock(TDQ_LOCKPTR((t)));                                       \
    }
#define TDQ_UNLOCK(t)                                                          \
    {                                                                          \
        spinlock_unlock(TDQ_LOCKPTR((t)));                                     \
        spinlock_exit();                                                       \
    }

#define TDA_SCHED 1

#define cpu_ticks() get_tick()
#define curthread ((thread_t *)m_curthread)

typedef struct runq_head {
    thread_t *first, **last;
} runq_head_t;

typedef struct runq {
    runq_head_t queues[64];
    uint64_t bits;
} runq_t;

typedef struct threadqueue {
    spinlock_t lock;
    runq_t realtime;
    runq_t timeshare;
    runq_t idle;
    uint8_t idx, ridx;
    int load;
    int lowpri;
    bool owepreempt;
    uint32_t transferable;
    int sysload;
    thread_t *tdq_curthread;
    int oldswitchcnt;
    int switchcnt;
    int cpu_idle;
} threadqueue_t;

enum td_states {
    TDS_INACTIVE = 0x0,
    TDS_INHIBITED,
    TDS_CAN_RUN,
    TDS_RUNQ,
    TDS_RUNNING
} td_state; /* (t) thread state */

static spinlock_t blocked_lock;
static CPU_LOCAL uintptr_t m_curthread;
static CPU_LOCAL uintptr_t m_idlethread;
static CPU_LOCAL uintptr_t frame = 0;
static CPU_LOCAL uint64_t m_switchtime;
static CPU_LOCAL uint64_t m_switchticks;
static CPU_LOCAL uintptr_t m_pcputicks;

static int ticks;
static int tickincr;
static int affinity;
static threadqueue_t *m_run_queues;

static int hz = 127;
static int stathz = 127;

static int realstathz = 127;    /* reset during boot. */
static int sched_slice = 10;    /* reset during boot. */
static int sched_slice_min = 1; /* reset during boot. */
static int preempt_thresh = PRI_MIN_KERN;

void critical_exit_preempt(void);

void mi_switch(int flags);
static void sched_setpreempt(int);
static void sched_priority(thread_t *);
static void sched_thread_priority(thread_t *, uint8_t);
static int sched_interact_score(thread_t *);
static void sched_interact_update(thread_t *);
static void sched_interact_fork(thread_t *);
static void sched_pctcpu_update(thread_t *, int);

static thread_t *tdq_choose(threadqueue_t *);
static void tdq_setup(threadqueue_t *, int i);
static void tdq_load_add(threadqueue_t *, thread_t *);
static void tdq_load_rem(threadqueue_t *, thread_t *);
static void tdq_runq_add(threadqueue_t *, thread_t *, int);
static void tdq_runq_rem(threadqueue_t *, thread_t *);
static inline int sched_shouldpreempt(int, int, int);
static void tdq_print(int cpu);
static void runq_print(struct runq *rq);
static int tdq_add(threadqueue_t *, thread_t *, int);
static int tdq_move(threadqueue_t *, threadqueue_t *);
static int tdq_idled(threadqueue_t *);
static void tdq_notify(threadqueue_t *, int lowpri);
static thread_t *tdq_steal(threadqueue_t *, int);
static thread_t *runq_steal(struct runq *, int);
static int sched_pickcpu(thread_t *, int);
static void sched_balance(void);
static bool sched_balance_pair(threadqueue_t *, threadqueue_t *);
static inline threadqueue_t *sched_setcpu(thread_t *, int, int);
static inline void thread_unblock_switch(thread_t *, spinlock_t *);

void sched_add(thread_t *td, int flags);
thread_t *sched_choose(void);
void sched_clock(thread_t *td, int cnt);
void sched_idletd(void *);
void sched_preempt(thread_t *td);
void sched_relinquish(thread_t *td);
void sched_rem(thread_t *td);
void sched_wakeup(thread_t *td, int srqflags);
thread_t *choosethread(void);

INTERRUPT void ast_sched_locked(thread_t *ctd, int ast) { ctd->sched_ast = 1; }
INTERRUPT void ast_unsched_locked(thread_t *ctd, int ast) {
    ctd->sched_ast = 0;
}

INTERRUPT static void critical_enter(void) {
    thread_t *td;
    td = (thread_t *)m_curthread;
    td->critnest++;
    atomic_signal_fence(memory_order_seq_cst);
}

INTERRUPT static void critical_exit(void) {
    thread_t *td;
    td = (thread_t *)m_curthread;
    ASSERT(td->critnest != 0, "critical_exit: td_critnest == 0");
    atomic_signal_fence(memory_order_seq_cst);
    td->critnest--;
    atomic_signal_fence(memory_order_seq_cst);
    if (td->owepreempt)
        critical_exit_preempt();
}

INTERRUPT void spinlock_enter(void) {
    thread_t *td;
    td = (void *)m_curthread;
    if (td->spinlock_count == 0) {
        td->spinlock_status = (__readeflags() & BIT9) ? true : false;
        if (td->spinlock_status)
            _disable();

        td->spinlock_count = 1;
        critical_enter();
    } else
        td->spinlock_count++;
}

INTERRUPT void spinlock_exit(void) {
    thread_t *td;
    td = (void *)m_curthread;
    td->spinlock_count--;
    if (td->spinlock_count == 0) {
        critical_exit();
        if (td->spinlock_status)
            _enable();
    }
}

INTERRUPT void cpu_switch(thread_t *old, thread_t *new, spinlock_t *mtx) {
    // printf("from %s to %s\n", old->name, new->name);

    ASSERT(frame != 0);
    interrupt_context_t *ctx = (void *)frame;
    frame = 0;
    lapic_set_timeout(10 * TICKS_PER_MILLISECOND);

    ASSERT(new->critnest == 1);
    // ASSERT(new->spinlock_count == 1);
    new->spinlock_count = 0;
    new->critnest = 0;

    save_thread_context(old, ctx);
    atomic_store_rel_ptr(&old->lock, mtx);
    while (atomic_load_explicit((_Atomic(spinlock_t *) *)(&new->lock),
                                memory_order_acquire) == &blocked_lock)
        ;
    m_curthread = (uintptr_t) new;
    new->oncpu = get_cpu_id();

    restore_thread_context(new, ctx);
    restore_interrupt_context(ctx);
}

INTERRUPT static threadqueue_t *get_run_queue() {
    return &m_run_queues[get_cpu_id()];
}

INTERRUPT static threadqueue_t *get_run_queue_of(int cpu_id) {
    ASSERT(cpu_id < get_cpu_count());
    return &m_run_queues[cpu_id];
}

INTERRUPT bool runq_head_empty(runq_head_t *rqh) { return rqh->first == NULL; }

INTERRUPT void runq_head_insh(runq_head_t *rqh, thread_t *td) {
    if ((td->next_in_bucket = rqh->first) != NULL) {
        rqh->first->prev_in_bucket = &td->next_in_bucket;
    } else {
        rqh->last = &td->next_in_bucket;
    }
    rqh->first = td;
    td->prev_in_bucket = &rqh->first;
}

INTERRUPT void runq_head_inst(runq_head_t *rqh, thread_t *td) {
    td->next_in_bucket = NULL;
    td->prev_in_bucket = rqh->last;
    *rqh->last = td;
    rqh->last = &td->next_in_bucket;
}

INTERRUPT void runq_head_remove(runq_head_t *rqh, thread_t *td) {
    if (td->next_in_bucket != NULL) {
        td->next_in_bucket->prev_in_bucket = td->prev_in_bucket;
    } else {
        rqh->last = td->prev_in_bucket;
    }
    *td->prev_in_bucket = td->next_in_bucket;
}

INTERRUPT static void runq_clrbit(runq_t *rq, int pri) {
    rq->bits &= ~(1ul << pri);
}

INTERRUPT static void runq_setbit(runq_t *rq, int pri) {
    rq->bits |= (1ul << pri);
}

INTERRUPT void runq_add(runq_t *rq, thread_t *td, int flags) {
    int pri = td->priority / RQ_PPQ;
    td->rqindex = pri;
    runq_setbit(rq, pri);
    runq_head_t *rqh = &rq->queues[pri];
    if (flags & SRQ_PREEMPTED) {
        runq_head_insh(rqh, td);
    } else {
        runq_head_inst(rqh, td);
    }
}

INTERRUPT void runq_add_pri(runq_t *rq, thread_t *td, uint8_t pri, int flags) {
    ASSERT(pri < RQ_NQS); // ("runq_add_pri: %d out of range", pri));
    td->rqindex = pri;
    runq_setbit(rq, pri);
    runq_head_t *rqh = &rq->queues[pri];
    if (flags & SRQ_PREEMPTED) {
        runq_head_insh(rqh, td);
    } else {
        runq_head_inst(rqh, td);
    }
}

INTERRUPT void runq_remove_idx(runq_t *rq, thread_t *td, uint8_t *idx) {
    uint8_t pri = td->rqindex;
    ASSERT(pri < RQ_NQS); // , ("runq_remove_idx: Invalid index %d\n", pri));
    runq_head_t *rqh = &rq->queues[pri];
    runq_head_remove(rqh, td);
    if (runq_head_empty(rqh)) {
        runq_clrbit(rq, pri);
        if (idx != NULL && *idx == pri)
            *idx = (pri + 1) % RQ_NQS;
    }
}

INTERRUPT void runq_remove(struct runq *rq, thread_t *td) {
    runq_remove_idx(rq, td, NULL);
}

INTERRUPT static int runq_findbit(runq_t *rq) {
    // TODO: optimize
    for (int i = 0; i < 64; i++) {
        int idx = i;
        if (rq->bits & (1ul << idx))
            return idx;
    }
    return -1;
}

INTERRUPT static int runq_findbit_from(runq_t *rq, uint8_t pri) {
    // TODO: optimize
    for (int i = 0; i < 64; i++) {
        int idx = (i + pri) % 64;
        if (rq->bits & (1ul << idx))
            return idx;
    }
    return -1;
}

INTERRUPT thread_t *runq_choose(runq_t *rq) {
    int pri;
    while ((pri = runq_findbit(rq)) != -1) {
        runq_head_t *rqh = &rq->queues[pri];
        thread_t *td = rqh->first;
        ASSERT(td != NULL, "runq_choose: no thread on busy queue");
        return (td);
    }
    return (NULL);
}

INTERRUPT thread_t *runq_choose_from(runq_t *rq, uint8_t idx) {
    int pri;
    if ((pri = runq_findbit_from(rq, idx)) != -1) {
        runq_head_t *rqh = &rq->queues[pri];
        thread_t *td = rqh->first;
        ASSERT(td != NULL, "runq_choose: no thread on busy queue");
        return (td);
    }
    return (NULL);
}

INTERRUPT spinlock_t *thread_lock_block(thread_t *td) {
    spinlock_t *lock;

    lock = td->lock;
    // mtx_assert(lock, MA_OWNED);
    td->lock = &blocked_lock;

    return (lock);
}

INTERRUPT void thread_lock_unblock(thread_t *td, spinlock_t *new) {
    // mtx_assert(new, MA_OWNED);
    ASSERT(td->lock == &blocked_lock, "thread %p lock %p not blocked_lock %p",
           td, td->lock, &blocked_lock);
    atomic_store_rel_ptr((volatile void *)&td->lock, (uintptr_t) new);
}

INTERRUPT static inline void thread_unblock_switch(thread_t *td,
                                                   spinlock_t *mtx) {
    atomic_store_rel_ptr((volatile uintptr_t *)&td->lock, (uintptr_t)mtx);
}

INTERRUPT static void tdq_runq_add(threadqueue_t *tdq, thread_t *td,
                                   int flags) {
    TDQ_LOCK_ASSERT(tdq);
    // THREAD_LOCK_BLOCKED_ASSERT(td, MA_OWNED);

    uint8_t pri = td->priority;
    TD_SET_RUNQ(td);

    /*if (THREAD_CAN_MIGRATE(td)) {
            tdq->tdq_transferable++;
            ts->ts_flags |= TSF_XFERABLE;
    }*/

    if (pri < PRI_MIN_BATCH) {
        td->ts_runq = &tdq->realtime;
    } else if (pri <= PRI_MAX_BATCH) {
        td->ts_runq = &tdq->timeshare;
        /*
         * This queue contains only priorities between MIN and MAX
         * batch.  Use the whole queue to represent these values.
         */
        ASSERT(pri <= PRI_MAX_BATCH && pri >= PRI_MIN_BATCH,
               "Invalid priority %d on timeshare runq", pri);
        if ((flags & (SRQ_BORROWING | SRQ_PREEMPTED)) == 0) {
            pri = RQ_NQS * (pri - PRI_MIN_BATCH) / PRI_BATCH_RANGE;
            pri = (pri + tdq->idx) % RQ_NQS;
            /*
             * This effectively shortens the queue by one so we
             * can have a one slot difference between idx and
             * ridx while we wait for threads to drain.
             */
            if (tdq->ridx != tdq->idx && pri == tdq->ridx)
                pri = (uint8_t)(pri - 1) % RQ_NQS;
        } else {
            pri = tdq->ridx;
        }

        runq_add_pri(td->ts_runq, td, pri, flags);
        return;
    } else {
        td->ts_runq = &tdq->idle;
    }
    runq_add(td->ts_runq, td, flags);
}

INTERRUPT static void tdq_runq_rem(threadqueue_t *tdq, thread_t *td) {
    TDQ_LOCK_ASSERT(tdq);
    // THREAD_LOCK_BLOCKED_ASSERT(td, MA_OWNED);
    ASSERT(td->ts_runq != NULL, "tdq_runq_remove: thread %p null ts_runq", td);
    /*if (td->ts_flags & TSF_XFERABLE) {
            tdq->transferable--;
            td->ts_flags &= ~TSF_XFERABLE;
    }*/

    if (td->ts_runq == &tdq->timeshare) {
        if (tdq->idx != tdq->ridx)
            runq_remove_idx(td->ts_runq, td, &tdq->ridx);
        else
            runq_remove_idx(td->ts_runq, td, NULL);
    } else
        runq_remove(td->ts_runq, td);
}

INTERRUPT static void tdq_load_add(threadqueue_t *tdq, thread_t *td) {
    TDQ_LOCK_ASSERT(tdq);
    // THREAD_LOCK_BLOCKED_ASSERT(td, MA_OWNED);
    tdq->load++;
    if ((td->flags & TDF_NOLOAD) == 0)
        tdq->sysload++;
}

INTERRUPT static void tdq_load_rem(threadqueue_t *tdq, thread_t *td) {
    TDQ_LOCK_ASSERT(tdq);
    // THREAD_LOCK_BLOCKED_ASSERT(td, MA_OWNED);
    ASSERT(tdq->load != 0);

    tdq->load--;
    if ((td->flags & TDF_NOLOAD) == 0)
        tdq->sysload--;
}

INTERRUPT static inline int tdq_slice(threadqueue_t *tdq) {
    int load;

    /*
     * It is safe to use sys_load here because this is called from
     * contexts where timeshare threads are running and so there
     * cannot be higher priority load in the system.
     */
    load = tdq->sysload - 1;
    if (load >= SCHED_SLICE_MIN_DIVISOR)
        return (sched_slice_min);
    if (load <= 1)
        return (sched_slice);
    return (sched_slice / load);
}

static inline int td_slice(thread_t *td, threadqueue_t *tdq) {
    if (td->pri_class == PRI_ITHD)
        return (sched_slice);
    return (tdq_slice(tdq));
}

INTERRUPT static void tdq_setlowpri(threadqueue_t *tdq, thread_t *ctd) {
    thread_t *td;

    // TDQ_LOCK_ASSERT(tdq, MA_OWNED);
    if (ctd == NULL)
        ctd = tdq->tdq_curthread;
    td = tdq_choose(tdq);
    if (td == NULL || td->priority > ctd->priority)
        tdq->lowpri = ctd->priority;
    else
        tdq->lowpri = td->priority;
}

INTERRUPT static void tdq_notify(threadqueue_t *tdq, int lowpri) {
    // int cpu;

    TDQ_LOCK_ASSERT(tdq);
    ASSERT(tdq->lowpri <= lowpri, "tdq_notify: lowpri %d > tdq_lowpri %d",
           lowpri, tdq->lowpri);

    if (tdq->owepreempt)
        return;

    /*
     * Check to see if the newly added thread should preempt the one
     * currently running.
     */
    if (!sched_shouldpreempt(tdq->lowpri, lowpri, 1))
        return;

    /*
     * Make sure that our caller's earlier update to tdq_load is
     * globally visible before we read tdq_cpu_idle.  Idle thread
     * accesses both of them without locks, and the order is important.
     */
    atomic_thread_fence(memory_order_seq_cst);

    // cpu = tdq->idx;
    //  TODO:
    // if (TD_IS_IDLETHREAD(tdq->tdq_curthread) &&
    //     (atomic_load_int(&tdq->cpu_idle) == 0 || cpu_idle_wakeup(cpu)))
    //   return;

    /*
     * The run queues have been updated, so any switch on the remote CPU
     * will satisfy the preemption request.
     */
    tdq->owepreempt = 1;

    // TODO: ipi_cpu(cpu, IPI_PREEMPT);
}

INTERRUPT static thread_t *tdq_choose(threadqueue_t *tdq) {
    thread_t *td;

    // TDQ_LOCK_ASSERT(tdq, MA_OWNED);
    td = runq_choose(&tdq->realtime);
    if (td != NULL)
        return (td);
    td = runq_choose_from(&tdq->timeshare, tdq->ridx);
    if (td != NULL) {
        ASSERT(td->priority >= PRI_MIN_BATCH,
               "tdq_choose: Invalid priority on timeshare queue %d",
               td->priority);
        return (td);
    }
    td = runq_choose(&tdq->idle);
    if (td != NULL) {
        ASSERT(td->priority >= PRI_MIN_IDLE,
               "tdq_choose: Invalid priority on idle queue %d", td->priority);
        return (td);
    }

    return (NULL);
}

INTERRUPT static int tdq_add(threadqueue_t *tdq, thread_t *td, int flags) {
    int lowpri;

    // TDQ_LOCK_ASSERT(tdq, MA_OWNED);
    // THREAD_LOCK_BLOCKED_ASSERT(td, MA_OWNED);
    ASSERT(td->inhibitors == 0, "sched_add: trying to run inhibited thread");
    ASSERT(TD_CAN_RUN(td) || TD_IS_RUNNING(td), "sched_add: bad thread state");
    // KASSERT(td->flags & TDF_INMEM,
    //     ("sched_add: thread swapped out"));

    lowpri = tdq->lowpri;
    if (td->priority < lowpri)
        tdq->lowpri = td->priority;
    tdq_runq_add(tdq, td, flags);
    tdq_load_add(tdq, td);
    return (lowpri);
}

INTERRUPT static inline threadqueue_t *sched_setcpu(thread_t *td, int cpu,
                                                    int flags) {

    threadqueue_t *tdq;
    spinlock_t *mtx;

    ////THREAD_LOCK_ASSERT(td, MA_OWNED);
    tdq = get_run_queue_of(cpu);
    td->ts_cpu = cpu;
    /*
     * If the lock matches just return the queue.
     */
    if (td->lock == TDQ_LOCKPTR(tdq)) {
        ASSERT((flags & SRQ_HOLD) == 0,
               "sched_setcpu: Invalid lock for SRQ_HOLD");
        return (tdq);
    }

    /*
     * The hard case, migration, we need to block the thread first to
     * prevent order reversals with other cpus locks.
     */
    spinlock_enter();
    mtx = thread_lock_block(td);
    if ((flags & SRQ_HOLD) == 0)
        spinlock_unlock(mtx);
    TDQ_LOCK(tdq);
    thread_lock_unblock(td, TDQ_LOCKPTR(tdq));
    spinlock_exit();
    return (tdq);
}

INTERRUPT static int sched_interact_score(thread_t *td) {
    int div;

    /*
     * The score is only needed if this is likely to be an interactive
     * task.  Don't go through the expense of computing it if there's
     * no chance.
     */
    if (SCHED_INTERACT_THRESH <= SCHED_INTERACT_HALF &&
        td->ts_runtime >= td->ts_slptime)
        return (SCHED_INTERACT_HALF);

    if (td->ts_runtime > td->ts_slptime) {
        div = MAX(1, td->ts_runtime / SCHED_INTERACT_HALF);
        return (SCHED_INTERACT_HALF +
                (SCHED_INTERACT_HALF - (td->ts_slptime / div)));
    }
    if (td->ts_slptime > td->ts_runtime) {
        div = MAX(1, td->ts_slptime / SCHED_INTERACT_HALF);
        return (td->ts_runtime / div);
    }
    /* ts_runtime == ts_slptime */
    if (td->ts_runtime)
        return (SCHED_INTERACT_HALF);

    /*
     * This can happen if ts_slptime and ts_runtime are 0.
     */
    return (0);
}

static int sched_pickcpu(thread_t *td, int flags) {
    threadqueue_t *tdq;
    int cpu, self;

    self = get_cpu_id();
    /*
     * Don't migrate a running thread from sched_switch().
     */
    if (flags & SRQ_OURSELF)
        return (td->ts_cpu);
    /*
     * Prefer to run interrupt threads on the processors that generate
     * the interrupt.
     */
    if (td->priority <= PRI_MAX_ITHD) {
        tdq = get_run_queue();
        if (tdq->lowpri >= PRI_MIN_IDLE) {
            return (self);
        }
        td->ts_cpu = self;
        goto llc;
    } else {
        tdq = get_run_queue_of(td->ts_cpu);
    }
    /*
     * If the thread can run on the last cpu and the affinity has not
     * expired and it is idle, run it there.
     */
    if (atomic_load_char(&tdq->lowpri) >= PRI_MIN_IDLE &&
        SCHED_AFFINITY(td, 2)) {
        return td->ts_cpu;
    }

llc:
    // Find least loaded cpu
    cpu = 0;
    int currload = get_run_queue_of(cpu)->load;
    for (int i = 0; i < get_cpu_count(); i++) {
        tdq = get_run_queue_of(cpu);
        int load = TDQ_LOAD(tdq);
        if (load < currload) {
            currload = load;
            cpu = i;
        }
    }

    /*
     * Compare the lowest loaded cpu to current cpu.
     */
    tdq = get_run_queue_of(cpu);
    if (get_run_queue()->lowpri > td->priority &&
        atomic_load_char(&tdq->lowpri) < PRI_MIN_IDLE &&
        TDQ_LOAD(get_run_queue()) <= TDQ_LOAD(tdq) + 1) {
        cpu = self;
    }
    return (cpu);
}

INTERRUPT static inline int sched_shouldpreempt(int pri, int cpri, int remote) {
    /*
     * If the new priority is not better than the current priority there is
     * nothing to do.
     */
    if (pri >= cpri)
        return (0);
    /*
     * Always preempt idle.
     */
    if (cpri >= PRI_MIN_IDLE)
        return (1);
    /*
     * If preemption is disabled don't preempt others.
     */
    if (preempt_thresh == 0)
        return (0);
    /*
     * Preempt if we exceed the threshold.
     */
    if (pri <= preempt_thresh)
        return (1);
    /*
     * If we're interactive or better and there is non-interactive
     * or worse running preempt only remote processors.
     */
    if (remote && pri <= PRI_MAX_INTERACT && cpri > PRI_MAX_INTERACT)
        return (1);
    return (0);
}

// TODO:
void sched_user_prio(thread_t *td, uint8_t prio) {
    td->base_user_pri = prio;
    // TODO:
    // if (td->lend_user_pri <= prio)
    //	return;
    td->user_pri = prio;
}

INTERRUPT static void sched_priority(thread_t *td) {
    uint32_t pri, score;

    if (td->pri_class != PRI_TIMESHARE)
        return;
    /*
     * If the score is interactive we place the thread in the realtime
     * queue with a priority that is less than kernel and interrupt
     * priorities.  These threads are not subject to nice restrictions.
     *
     * Scores greater than this are placed on the normal timeshare queue
     * where the priority is partially decided by the most recent cpu
     * utilization and the rest is decided by nice value.
     *
     * The nice value of the process has a linear effect on the calculated
     * score.  Negative nice values make it easier for a thread to be
     * considered interactive.
     */
    score = MAX(0, sched_interact_score(td)); // + td->proc->p_nice);
    if (score < SCHED_INTERACT_THRESH) {
        pri = PRI_MIN_INTERACT;
        pri += (PRI_MAX_INTERACT - PRI_MIN_INTERACT + 1) * score /
               SCHED_INTERACT_THRESH;
        ASSERT(pri >= PRI_MIN_INTERACT && pri <= PRI_MAX_INTERACT,
               "sched_priority: invalid interactive priority %u score %u", pri,
               score);
    } else {
        pri = SCHED_PRI_MIN;
        if (td->ts_ticks)
            pri += MIN(SCHED_PRI_TICKS(td), SCHED_PRI_RANGE - 1);
        // TODO: pri += SCHED_PRI_NICE(td->proc->p_nice);
        ASSERT(pri >= PRI_MIN_BATCH && pri <= PRI_MAX_BATCH);
    }
    sched_user_prio(td, pri);

    return;
}

INTERRUPT static void sched_interact_update(thread_t *td) {
    uint32_t sum;

    sum = td->ts_runtime + td->ts_slptime;
    if (sum < SCHED_SLP_RUN_MAX)
        return;
    /*
     * This only happens from two places:
     * 1) We have added an unusual amount of run time from fork_exit.
     * 2) We have added an unusual amount of sleep time from sched_sleep().
     */
    if (sum > SCHED_SLP_RUN_MAX * 2) {
        if (td->ts_runtime > td->ts_slptime) {
            td->ts_runtime = SCHED_SLP_RUN_MAX;
            td->ts_slptime = 1;
        } else {
            td->ts_slptime = SCHED_SLP_RUN_MAX;
            td->ts_runtime = 1;
        }
        return;
    }
    /*
     * If we have exceeded by more than 1/5th then the algorithm below
     * will not bring us back into range.  Dividing by two here forces
     * us into the range of [4/5 * SCHED_INTERACT_MAX, SCHED_INTERACT_MAX]
     */
    if (sum > (SCHED_SLP_RUN_MAX / 5) * 6) {
        td->ts_runtime /= 2;
        td->ts_slptime /= 2;
        return;
    }
    td->ts_runtime = (td->ts_runtime / 5) * 4;
    td->ts_slptime = (td->ts_slptime / 5) * 4;
}

INTERRUPT static void sched_pctcpu_update(thread_t *td, int run) {
    int t = ticks;

    /*
     * The signed difference may be negative if the thread hasn't run for
     * over half of the ticks rollover period.
     */
    if ((uint32_t)(t - td->ts_ltick) >= SCHED_TICK_TARG) {
        td->ts_ticks = 0;
        td->ts_ftick = t - SCHED_TICK_TARG;
    } else if (t - td->ts_ftick >= SCHED_TICK_MAX) {
        td->ts_ticks = (td->ts_ticks / (td->ts_ltick - td->ts_ftick)) *
                       (td->ts_ltick - (t - SCHED_TICK_TARG));
        td->ts_ftick = t - SCHED_TICK_TARG;
    }
    if (run)
        td->ts_ticks += (t - td->ts_ltick) << SCHED_TICK_SHIFT;
    td->ts_ltick = t;
}

INTERRUPT static void sched_thread_priority(thread_t *td, uint8_t prio) {
    threadqueue_t *tdq;
    int oldpri;

    ////THREAD_LOCK_ASSERT(td, MA_OWNED);
    if (td->priority == prio)
        return;
    /*
     * If the priority has been elevated due to priority
     * propagation, we may have to move ourselves to a new
     * queue.  This could be optimized to not re-add in some
     * cases.
     */
    if (TD_ON_RUNQ(td) && prio < td->priority) {
        sched_rem(td);
        td->priority = prio;
        sched_add(td, SRQ_BORROWING | SRQ_HOLDTD);
        return;
    }
    /*
     * If the thread is currently running we may have to adjust the lowpri
     * information so other cpus are aware of our current priority.
     */
    if (TD_IS_RUNNING(td)) {
        tdq = get_run_queue_of(td->ts_cpu);
        oldpri = td->priority;
        td->priority = prio;
        if (prio < tdq->lowpri)
            tdq->lowpri = prio;
        else if (tdq->lowpri == oldpri)
            tdq_setlowpri(tdq, td);
        return;
    }
    td->priority = prio;
}

INTERRUPT void sched_lend_prio(thread_t *td, uint8_t prio) {
    td->flags |= TDF_BORROWING;
    sched_thread_priority(td, prio);
}

INTERRUPT void sched_unlend_prio(thread_t *td, uint8_t prio) {
    uint8_t base_pri;

    if (td->base_pri >= PRI_MIN_TIMESHARE && td->base_pri <= PRI_MAX_TIMESHARE)
        base_pri = td->user_pri;
    else
        base_pri = td->base_pri;
    if (prio >= base_pri) {
        td->flags &= ~TDF_BORROWING;
        sched_thread_priority(td, base_pri);
    } else
        sched_lend_prio(td, prio);
}

INTERRUPT void sched_prio(thread_t *td, uint8_t prio) {
    /* First, update the base priority. */
    td->base_pri = prio;

    /*
     * If the thread is borrowing another thread's priority, don't
     * ever lower the priority.
     */
    if (td->flags & TDF_BORROWING && td->priority < prio)
        return;

    /* Change the real priority. */
    sched_thread_priority(td, prio);

    ///*
    // * If the thread is on a turnstile, then let the turnstile update
    // * its state.
    // */
    // if (TD_ON_LOCK(td) && oldprio != prio)
    //	turnstile_adjust(td, oldprio);
}

void sched_lend_user_prio(thread_t *td, uint8_t prio) {

    // THREAD_LOCK_ASSERT(td, MA_OWNED);
    td->lend_user_pri = prio;
    td->user_pri = MIN(prio, td->base_user_pri);
    if (td->priority > td->user_pri)
        sched_prio(td, td->user_pri);
    else if (td->priority != td->user_pri)
        ast_sched_locked(td, TDA_SCHED);
}

/*
 * Like the above but first check if there is anything to do.
 */
void sched_lend_user_prio_cond(thread_t *td, uint8_t prio) {

    if (td->lend_user_pri != prio)
        goto lend;
    if (td->user_pri != MIN(prio, td->base_user_pri))
        goto lend;
    if (td->priority != td->user_pri)
        goto lend;
    return;

lend:
    thread_lock(td);
    sched_lend_user_prio(td, prio);
    thread_unlock(td);
}

INTERRUPT static spinlock_t *sched_switch_migrate(threadqueue_t *tdq,
                                                  thread_t *td, int flags) {
    threadqueue_t *tdn;
    int lowpri;
    ASSERT((td->flags & TSF_BOUND) != 0, "Thread %p shouldn't migrate", td);

    tdn = get_run_queue_of(td->ts_cpu);
    tdq_load_rem(tdq, td);
    /*
     * Do the lock dance required to avoid LOR.  We have an
     * extra spinlock nesting from sched_switch() which will
     * prevent preemption while we're holding neither run-queue lock.
     */
    TDQ_UNLOCK(tdq);
    TDQ_LOCK(tdn);
    lowpri = tdq_add(tdn, td, flags);
    tdq_notify(tdn, lowpri);
    TDQ_UNLOCK(tdn);
    TDQ_LOCK(tdq);
    return (TDQ_LOCKPTR(tdn));
}

INTERRUPT void sched_switch(thread_t *td, int flags) {
    thread_t *newtd;
    threadqueue_t *tdq;
    spinlock_t *mtx;
    int srqflag;
    int cpuid, preempted;
    int pickcpu;

    // THREAD_LOCK_ASSERT(td, MA_OWNED);

    cpuid = get_cpu_id();
    tdq = get_run_queue();
    sched_pctcpu_update(td, 1);
    pickcpu = (td->flags & TDF_PICKCPU) != 0;
    if (pickcpu)
        td->ts_rltick = ticks - affinity * MAX_CACHE_LEVELS;
    else
        td->ts_rltick = ticks;
    td->lastcpu = td->oncpu;
    preempted = (td->flags & TDF_SLICEEND) == 0 && (flags & SW_PREEMPT) != 0;
    td->flags &= ~(TDF_PICKCPU | TDF_SLICEEND);
    ast_unsched_locked(td, TDA_SCHED);
    td->owepreempt = 0;
    atomic_store_char(&tdq->owepreempt, 0);
    if (!TD_IS_IDLETHREAD(td))
        TDQ_SWITCHCNT_INC(tdq);

    /*
     * Always block the thread lock so we can drop the tdq lock early.
     */
    mtx = thread_lock_block(td);
    spinlock_enter();
    if (TD_IS_IDLETHREAD(td)) {
        // MPASS(mtx == TDQ_LOCKPTR(tdq));
        TD_SET_CAN_RUN(td);
    } else if (TD_IS_RUNNING(td)) {
        // MPASS(mtx == TDQ_LOCKPTR(tdq));
        srqflag = preempted ? SRQ_OURSELF | SRQ_YIELDING | SRQ_PREEMPTED
                            : SRQ_OURSELF | SRQ_YIELDING;
        if (pickcpu)
            td->ts_cpu = sched_pickcpu(td, 0);
        if (td->ts_cpu == cpuid)
            tdq_runq_add(tdq, td, srqflag);
        else
            mtx = sched_switch_migrate(tdq, td, srqflag);
    } else {
        /* This thread must be going to sleep. */
        if (mtx != TDQ_LOCKPTR(tdq)) {
            spinlock_unlock(mtx);
            TDQ_LOCK(tdq);
        }
        tdq_load_rem(tdq, td);
#ifdef SMP
        if (tdq->tdq_load == 0)
            tdq_trysteal(tdq);
#endif
    }

    /*
     * We enter here with the thread blocked and assigned to the
     * appropriate cpu run-queue or sleep-queue and with the current
     * thread-queue locked.
     */
    // TDQ_LOCK_ASSERT(tdq, MA_OWNED | MA_NOTRECURSED);
    // MPASS(td == tdq->tdq_curthread);
    newtd = choosethread();
    sched_pctcpu_update(newtd, 0);
    TDQ_UNLOCK(tdq);

    /*
     * Call the MD code to switch contexts if necessary.
     */
    td->oncpu = NOCPU;
    cpu_switch(td, newtd, mtx);

    // cpuid = td->oncpu = get_cpu_id();
    // ASSERT(curthread->td_md.md_spinlock_count == 1,
    //     ("invalid count %d", curthread->td_md.md_spinlock_count));
}

INTERRUPT void sched_sleep(thread_t *td, int prio) {
    ////THREAD_LOCK_ASSERT(td, MA_OWNED);

    td->slptick = ticks;
    // if (TD_IS_SUSPENDED(td) || prio >= PSOCK)
    //	td->flags |= TDF_CANSWAP;
    if (td->pri_class != PRI_TIMESHARE)
        return;
    if (td->priority > PRI_MIN_BATCH)
        sched_prio(td, PRI_MIN_BATCH);
}

INTERRUPT void sched_wakeup(thread_t *td, int srqflags) {
    int slptick;

    // THREAD_LOCK_ASSERT(td, MA_OWNED);
    // td->flags &= ~TDF_CANSWAP;

    /*
     * If we slept for more than a tick update our interactivity and
     * priority.
     */
    slptick = td->slptick;
    td->slptick = 0;
    if (slptick && slptick != ticks) {
        td->ts_slptime += (ticks - slptick) << SCHED_TICK_SHIFT;
        sched_interact_update(td);
        sched_pctcpu_update(td, 0);
    }

    /*
     * When resuming an idle ithread, restore its base ithread
     * priority.
     */
    if (td->pri_class == PRI_ITHD && td->priority != td->base_ithread_pri)
        sched_prio(td, td->base_ithread_pri);

    /*
     * Reset the slice value since we slept and advanced the round-robin.
     */
    td->ts_slice = 0;
    sched_add(td, SRQ_BORING | srqflags);
}

INTERRUPT void sched_preempt(thread_t *td) {
    threadqueue_t *tdq;
    int flags;

    thread_lock(td);
    tdq = get_run_queue();
    // TDQ_LOCK_ASSERT(tdq, MA_OWNED);
    if (td->priority > tdq->lowpri) {
        if (td->critnest == 1) {
            flags = SW_INVOL | SW_PREEMPT;
            flags |=
                TD_IS_IDLETHREAD(td) ? SWT_REMOTEWAKEIDLE : SWT_REMOTEPREEMPT;
            mi_switch(flags);
            /* Switch dropped thread lock. */
            return;
        }
        td->owepreempt = 1;
    } else {
        tdq->owepreempt = 0;
    }
    thread_unlock(td);
}

INTERRUPT thread_t *sched_choose(void) {
    thread_t *td;
    threadqueue_t *tdq;

    tdq = get_run_queue();
    // TDQ_LOCK_ASSERT(tdq, MA_OWNED);
    td = tdq_choose(tdq);
    if (td != NULL) {
        tdq_runq_rem(tdq, td);
        tdq->lowpri = td->priority;
    } else {
        tdq->lowpri = PRI_MAX_IDLE;
        td = (thread_t *)m_idlethread;
    }
    tdq->tdq_curthread = td;
    return (td);
}

INTERRUPT static void sched_setpreempt(int pri) {
    thread_t *ctd;
    int cpri;

    ctd = curthread;
    // THREAD_LOCK_ASSERT(ctd, MA_OWNED);

    cpri = ctd->priority;
    if (pri < cpri)
        ast_sched_locked(ctd, TDA_SCHED);
    if (pri >= cpri || TD_IS_INHIBITED(ctd))
        return;
    if (!sched_shouldpreempt(pri, cpri, 0))
        return;
    ctd->owepreempt = 1;
}

INTERRUPT void sched_add(thread_t *td, int flags) {
    threadqueue_t *tdq;
    int cpu, lowpri;
    // THREAD_LOCK_ASSERT(td, MA_OWNED);
    /*
     * Recalculate the priority before we select the target cpu or
     * run-queue.
     */
    if (td->pri_class == PRI_TIMESHARE)
        sched_priority(td);
    /*
     * Pick the destination cpu and if it isn't ours transfer to the
     * target cpu.
     */
    cpu = sched_pickcpu(td, flags);
    tdq = sched_setcpu(td, cpu, flags);
    lowpri = tdq_add(tdq, td, flags);
    if (cpu != get_cpu_id())
        tdq_notify(tdq, lowpri);
    else if (!(flags & SRQ_YIELDING))
        sched_setpreempt(td->priority);

    if (!(flags & SRQ_HOLDTD))
        thread_unlock(td);
}

INTERRUPT void sched_rem(thread_t *td) {
    threadqueue_t *tdq;

    tdq = get_run_queue_of(td->ts_cpu);
    // TDQ_LOCK_ASSERT(tdq, MA_OWNED);
    // MPASS(td->lock == TDQ_LOCKPTR(tdq));
    ASSERT(TD_ON_RUNQ(td), "sched_rem: thread not on run queue");
    tdq_runq_rem(tdq, td);
    tdq_load_rem(tdq, td);
    TD_SET_CAN_RUN(td);
    if (td->priority == tdq->lowpri)
        tdq_setlowpri(tdq, NULL);
}

INTERRUPT uint32_t sched_pctcpu(thread_t *td) {
    uint32_t pctcpu;
    pctcpu = 0;

    // THREAD_LOCK_ASSERT(td, MA_OWNED);
    sched_pctcpu_update(td, TD_IS_RUNNING(td));
    if (td->ts_ticks) {
        int rtick;

        /* How many rtick per second ? */
        rtick = MIN(SCHED_TICK_HZ(td) / SCHED_TICK_SECS, hz);
        pctcpu = (FSCALE * ((FSCALE * rtick) / hz)) >> FSHIFT;
    }

    return (pctcpu);
}

INTERRUPT static thread_t *sched_throw_grab(threadqueue_t *tdq) {
    thread_t *newtd;

    newtd = choosethread();
    spinlock_enter();
    TDQ_UNLOCK(tdq);
    ASSERT(curthread->spinlock_count == 1, "invalid count %d",
           curthread->spinlock_count);
    return (newtd);
}

INTERRUPT void sched_throw(thread_t *td) {
    thread_t *newtd;
    threadqueue_t *tdq;

    tdq = get_run_queue();

    // MPASS(td != NULL);
    // THREAD_LOCK_ASSERT(td, MA_OWNED);
    // THREAD_LOCKPTR_ASSERT(td, TDQ_LOCKPTR(tdq));

    tdq_load_rem(tdq, td);
    td->lastcpu = td->oncpu;
    td->oncpu = NOCPU;
    thread_lock_block(td);

    newtd = sched_throw_grab(tdq);

    /* doesnt return */
    cpu_switch(td, newtd, TDQ_LOCKPTR(tdq));
}

INTERRUPT void sched_clock(thread_t *td, int cnt) {
    threadqueue_t *tdq;

    // THREAD_LOCK_ASSERT(td, MA_OWNED);
    tdq = get_run_queue();
#ifdef SMP
    /*
     * We run the long term load balancer infrequently on the first cpu.
     */
    if (balance_tdq == tdq && smp_started != 0 && rebalance != 0 &&
        balance_ticks != 0) {
        balance_ticks -= cnt;
        if (balance_ticks <= 0)
            sched_balance();
    }
#endif
    /*
     * Save the old switch count so we have a record of the last ticks
     * activity.   Initialize the new switch count based on our load.
     * If there is some activity seed it to reflect that.
     */
    tdq->oldswitchcnt = tdq->switchcnt;
    tdq->switchcnt = tdq->load;

    /*
     * Advance the insert index once for each tick to ensure that all
     * threads get a chance to run.
     */
    if (tdq->idx == tdq->ridx) {
        tdq->idx = (tdq->idx + 1) % RQ_NQS;
        if ((tdq->timeshare.queues[tdq->ridx].first) == NULL)
            tdq->ridx = tdq->idx;
    }
    sched_pctcpu_update(td, 1);
    if (/*(td->pri_class & PRI_FIFO_BIT) ||*/ TD_IS_IDLETHREAD(td))
        return;

    if (td->pri_class == PRI_TIMESHARE) {
        /*
         * We used a tick; charge it to the thread so
         * that we can compute our interactivity.
         */
        td->ts_runtime += tickincr * cnt;
        sched_interact_update(td);
        sched_priority(td);
    }

    /*
     * Force a context switch if the current thread has used up a full
     * time slice (default is 100ms).
     */
    td->ts_slice += cnt;
    if (td->ts_slice >= td_slice(td, tdq)) {
        td->ts_slice = 0;

        /*
         * If an ithread uses a full quantum, demote its
         * priority and preempt it.
         */
        if (td->pri_class == PRI_ITHD) {
            td->owepreempt = 1;
            if (td->base_pri + RQ_PPQ < PRI_MAX_ITHD) {
                sched_prio(td, td->base_pri + RQ_PPQ);
            }
        } else {
            ast_sched_locked(td, TDA_SCHED);
            td->flags |= TDF_SLICEEND;
        }
    }
}

INTERRUPT static void ast_scheduler(thread_t *td) {
    thread_lock(td);
    sched_prio(td, td->user_pri);
    mi_switch(SW_INVOL | SWT_NEEDRESCHED);
}

INTERRUPT thread_t *choosethread(void) {
    thread_t *td = sched_choose();
    TD_SET_RUNNING(td);
    return td;
}

INTERRUPT void mi_switch(int flags) {
    uint64_t runtime, new_switchtime;
    struct thread *td;

    td = curthread; /* XXX */
    // THREAD_LOCK_ASSERT(td, MA_OWNED | MA_NOTRECURSED);
    ASSERT(!TD_ON_RUNQ(td), "mi_switch: called by old code");
    ASSERT(td->critnest == 1, "mi_switch: switch in a critical section");
    ASSERT((flags & (SW_INVOL | SW_VOL)) != 0,
           "mi_switch: switch must be voluntary or involuntary");

    if (flags & SW_VOL) {
        td->swvoltick = ticks;
    } else {
        td->swinvoltick = ticks;
    }
    /*
     * Compute the amount of time during which the current
     * thread was running, and add that to its total so far.
     */
    new_switchtime = cpu_ticks();
    runtime = new_switchtime - m_switchtime;
    td->runtime += runtime;
    td->incruntime += runtime;
    m_switchtime = new_switchtime;
    // td->generation++;	/* bump preempt-detect counter */
    m_switchticks = ticks;
    sched_switch(td, flags);

    /*
     * If the last thread was exiting, finish cleaning it up.
     */
    // TODO: deadthreads
    // spinlock_exit();
}

INTERRUPT void statclock(int cnt, int usermode) {
    thread_t *td;
    uint64_t runtime, new_switchtime;

    td = curthread;

    thread_lock(td);

    /*
     * Compute the amount of time during which the current
     * thread was running, and add that to its total so far.
     */
    new_switchtime = cpu_ticks();
    runtime = new_switchtime - m_switchtime;
    td->runtime += runtime;
    td->incruntime += runtime;
    m_switchtime = new_switchtime;

    sched_clock(td, cnt);
    thread_unlock(td);
}

static void sched_initticks(void *dummy) {
    int incr;

    realstathz = stathz ? stathz : hz;
    sched_slice = realstathz / SCHED_SLICE_DEFAULT_DIVISOR;
    sched_slice_min = sched_slice / SCHED_SLICE_MIN_DIVISOR;

    /*
     * tickincr is shifted out by 10 to avoid rounding errors due to
     * hz not being evenly divisible by stathz on all platforms.
     */
    incr = (hz << SCHED_TICK_SHIFT) / realstathz;
    /*
     * This does not work for values of stathz that are more than
     * 1 << SCHED_TICK_SHIFT * hz.  In practice this does not happen.
     */
    if (incr == 0)
        incr = 1;
    tickincr = incr;

    /*
     * Set the default balance interval now that we know
     * what realstathz is.
     */
    // balance_interval = realstathz;
    // balance_ticks = balance_interval;
    affinity = SCHED_AFFINITY_DEFAULT;
}

void hardclock(int cnt, int usermode) {
    int *t = (int *)m_pcputicks;
    int global, newticks;

    /*
     * Update per-CPU and possibly global ticks values.
     */
    *t += cnt;
    global = ticks;
    do {
        newticks = *t - global;
        if (newticks <= 0) {
            if (newticks < -1)
                *t = global - 1;
            newticks = 0;
            break;
        }
    } while (
        !atomic_compare_exchange_strong((_Atomic(int) *)&ticks, &global, *t));
}

INTERRUPT void scheduler_on_schedule(interrupt_context_t *ctx) {
    frame = (uintptr_t)ctx;
    long now = 0, poll_until = 0;
    check_timers(get_cpu_id(), &now, &poll_until, NULL);
    lapic_set_timeout(10 * TICKS_PER_MILLISECOND);

    hardclock(1, 0);
    statclock(1, 0);

    // if (((thread_t *)m_curthread)->sched_ast == 1)
    ast_scheduler((void *)m_curthread);
    ASSERT(false, "shouldn't be here");
    _disable();
    while (1)
        ;
}

INTERRUPT void scheduler_on_park(interrupt_context_t *ctx) {
    frame = (uintptr_t)ctx;

    // lapic_set_deadline(0);
    thread_t *td = curthread;
    thread_lock(td);
    TD_SET_SLEEPING(td);
    sched_sleep(td, 0);

    // check if we need to call a callback before we schedule
    if (ctx->rdi != 0) {
        ((void (*)(uint64_t))ctx->rdi)(ctx->rsi);
    }

    mi_switch(SW_VOL); // TODO:

    ASSERT(false, "shouldn't be here");
    _disable();
    while (1)
        ;
}

INTERRUPT void scheduler_on_drop(interrupt_context_t *ctx) {
    frame = (uintptr_t)ctx;
    frame = (uintptr_t)ctx;

    // lapic_set_deadline(0);
    thread_t *td = curthread;
    thread_lock(td);
    TD_SET_STATE(td, TDS_INACTIVE);

    mi_switch(SW_VOL); // TODO:

    ASSERT(false, "shouldn't be here");
    _disable();
    while (1)
        ;
}

bool scheduler_can_spin(int i) { return false; }

void critical_exit_preempt(void) {
    thread_t *td;
    // int flags;

    /*
     * If td_critnest is 0, it is possible that we are going to get
     * preempted again before reaching the code below. This happens
     * rarely and is harmless. However, this means td_owepreempt may
     * now be unset.
     */
    td = curthread;
    if (td->critnest != 0)
        return;

    /*
     * Microoptimization: we committed to switch,
     * disable preemption in interrupt handlers
     * while spinning for the thread lock.
     */
    // td->critnest = 1;
    // thread_lock(td);
    // td->critnest--;
    // flags = SW_INVOL | SW_PREEMPT;
    //	if (TD_IS_IDLETHREAD(td))
    //		flags |= SWT_IDLE;
    //	else
    //		flags |= SWT_OWEPREEMPT;
    // mi_switch(flags);
}

void sched_class(thread_t *td, int class) {
    // THREAD_LOCK_ASSERT(td, MA_OWNED);
    if (td->pri_class == class)
        return;
    td->pri_class = class;
}

void sched_new_thread(thread_t *td) {
    threadqueue_t *tdq = get_run_queue();

    // TODO: inherit from parent

    td->pri_class = PRI_TIMESHARE;
    td->user_pri = PRI_MIN_TIMESHARE;
    td->priority = PRI_MIN_KERN + 4;
    td->base_pri = PRI_MIN_KERN + 4;
    td->flags = 0;

    td->runtime = 0;
    td->ts_runq = tdq;
    td->lock = &tdq->lock;
    td->oncpu = NOCPU;
    td->lastcpu = NOCPU;
    td->ts_cpu = 0;
    td->flags = 0;

    td->spinlock_count = 1;
    td->critnest = 1;
}

void scheduler_wake_poller(int64_t when) { ASSERT("notimplemented"); }

void scheduler_resume_thread(suspend_state_t state) {
    ASSERT("notimplemented");
}
suspend_state_t scheduler_suspend_thread(thread_t *thread) {
    ASSERT("notimplemented");
    while (1)
        ;
}

thread_t *get_current_thread() { return (void *)m_curthread; }

void scheduler_preempt_disable(void) {
    if (m_curthread)
        spinlock_enter();
}

void scheduler_preempt_enable(void) {
    if (m_curthread)
        spinlock_exit();
}

//----------------------------------------------------------------------------------------------------------------------
// Interrupts to call the scheduler
//----------------------------------------------------------------------------------------------------------------------

void scheduler_yield() {
    // don't preempt if we can't preempt
    // if (m_preempt_disable_depth > 0) {
    //    return;
    //}

    __asm__ volatile("int %0" : : "i"(IRQ_YIELD) : "memory");
}

void scheduler_on_ready_thread(interrupt_context_t *ctx) {
    frame = (uintptr_t)ctx;

    thread_t *thread = (void *)ctx->rdi;
    thread_lock(thread);
    TD_CLR_SLEEPING(thread);
    TD_SET_CAN_RUN(thread);
    sched_wakeup(thread, 0);
}

void scheduler_ready_thread(thread_t *thread) {
    thread_lock(thread);
    TD_CLR_SLEEPING(thread);
    TD_SET_CAN_RUN(thread);
    sched_wakeup(thread, 0);
    if (thread->owepreempt)
        printf("SHOULD YIELD\n");
    // if (frame == 0) scheduler_yield();
    //__asm__ volatile("int %0"
    //                  :
    //                  : "i"(IRQ_READY_THREAD), "D"(thread)
    //                  : "memory");
}
void scheduler_park(void (*callback)(void *arg), void *arg) {
    __asm__ volatile("int %0"
                     :
                     : "i"(IRQ_PARK), "D"(callback), "S"(arg)
                     : "memory");
}

void scheduler_drop_current() {
    __asm__ volatile("int %0" : : "i"(IRQ_DROP) : "memory");
}

void idle_loop(void *ctx) {
    while (1) {
        asm("sti");
        uint64_t deadline = get_total_tick() + TICKS_PER_MILLISECOND;
        lapic_set_deadline(deadline);
        while (get_total_tick() < deadline)
            asm("hlt");
    }
}
void scheduler_startup() {
    // set to normal running priority
    __writecr8(PRIORITY_NORMAL);

    // enable interrupts
    _enable();

    // drop the current thread in favor of starting
    // the scheduler
    // scheduler_drop_current();
    interrupt_context_t ctx;
    restore_thread_context((thread_t *)m_idlethread, &ctx);
    restore_interrupt_context(&ctx);
}

err_t init_scheduler() {
    err_t err = NO_ERROR;

    m_run_queues = malloc(get_cpu_count() * sizeof(threadqueue_t));

    CHECK(m_run_queues != NULL);
    blocked_lock = INIT_SPINLOCK();
    // TODO: not rely on malloc zeroing
    for (int i = 0; i < get_cpu_count(); i++) {
        for (int j = 0; j < 64; j++) {
            m_run_queues[i].realtime.queues[j].first = NULL;
            m_run_queues[i].realtime.queues[j].last =
                &m_run_queues[i].realtime.queues[j].first;
            m_run_queues[i].timeshare.queues[j].first = NULL;
            m_run_queues[i].timeshare.queues[j].last =
                &m_run_queues[i].timeshare.queues[j].first;
            m_run_queues[i].idle.queues[j].first = NULL;
            m_run_queues[i].idle.queues[j].last =
                &m_run_queues[i].idle.queues[j].first;
            m_run_queues[i].lock = INIT_SPINLOCK();
        }
    }
    sched_initticks(NULL);

    // and set the scheduler stack
    CHECK_AND_RETHROW(init_scheduler_per_core());

cleanup:
    return err;
}

err_t init_scheduler_per_core() {
    err_t err = NO_ERROR;
    m_switchtime = cpu_ticks();

    thread_t *idlethread =
        create_thread(idle_loop, NULL, "kernel/idlethread%d", get_cpu_id());
    idlethread->flags = TDF_IDLETD | TDF_NOLOAD;
    TD_SET_CAN_RUN(idlethread);
    idlethread->spinlock_count = 0;
    idlethread->critnest = 0;
    idlethread->priority = PRI_MAX_IDLE;
    idlethread->pri_class = PRI_IDLE;
    idlethread->base_pri = PRI_MAX_IDLE;
    idlethread->base_user_pri = PRI_MAX_IDLE;

    m_pcputicks = (uintptr_t)palloc(sizeof(int));
    m_switchtime = cpu_ticks();
    m_idlethread = (uintptr_t)idlethread;
    m_curthread = (uintptr_t)idlethread;

cleanup:
    return err;
}
