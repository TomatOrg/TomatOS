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

#include "cpu_local.h"
#include "timer.h"
#include "util/defs.h"
#include "waitable.h"
#include "sync/irq_spinlock.h"
#include "mem/mem.h"
#include "irq/irq.h"
#include "mem/stack.h"
#include "arch/gdt.h"

#include <sync/spinlock.h>
#include <util/fastrand.h>
#include <arch/intrin.h>
#include <util/stb_ds.h>
#include <time/tsc.h>
#include <arch/apic.h>
#include <arch/msr.h>
#include <kernel.h>

#include <stdatomic.h>

// Range of the score() function
// Keep it 0-100 to match FreeBSD's
#define SCORE_RANGE_MAX 100


// CPU is out of work and is actively looking for work
static bool CPU_LOCAL m_spinning;
// Time of last poll, 0 if currently offline
static _Atomic(int64_t) m_last_poll;

// Time to which current poll is sleeping
static _Atomic(int64_t) m_poll_until;

// Number of spinning CPUs in the system
static _Atomic(uint32_t) m_number_spinning = 0;

// The CPU that's currently polling
static _Atomic(int) m_polling_cpu = -1;

// Is the current CPU waiting in a HLT?
static bool CPU_LOCAL m_waiting_for_irq;

// The idle cpus
static _Atomic(size_t) m_idle_cpus[256 / (sizeof(size_t) * 8)];
static _Atomic(uint32_t) m_idle_cpus_count;

// spinlock to protect the scheduler internal stuff
static irq_spinlock_t m_scheduler_lock = INIT_IRQ_SPINLOCK();

INTERRUPT static void lock_scheduler() {
    irq_spinlock_lock(&m_scheduler_lock);
}

INTERRUPT static void unlock_scheduler() {
    irq_spinlock_unlock(&m_scheduler_lock);
}

/**
 * Tried to wake a cpu for running threads
 */
INTERRUPT static void wake_cpu() {
    if (atomic_load(&m_idle_cpus_count) == 0) {
        return;
    }

    // get an idle cpu from the queue
    lock_scheduler();
    int cpu_id = -1;
    size_t mask_entries = ALIGN_UP(get_cpu_count(), sizeof(size_t) * 8) / (sizeof(size_t) * 8);
    for (int i = 0; i < mask_entries; i++) {
        int idle_mask = atomic_load(&m_idle_cpus[i]);
        if (idle_mask == 0) continue;
        cpu_id = __builtin_ffs(idle_mask) - 1;
    }
    unlock_scheduler();

    if (cpu_id == -1) {
        // no cpu to wake up
        return;
    } else {
        // send an ipi to schedule threads from the global run queue
        // to the found cpu
        lapic_send_ipi(IRQ_WAKEUP, cpu_id);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Local run queue
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct runq_head {
    thread_t *first, **last;
} runq_head_t;

typedef struct runq {
    runq_head_t queues[64];
    uint64_t bits;
} runq_t;

typedef struct threadqueue_t {
    spinlock_t lock;
    runq_t realtime;
    runq_t timeshare;
    runq_t idle;
    uint8_t idx, ridx;
    int load;
    int lowpri;
    bool owepreempt;
} threadqueue_t;

#define SRQ_PREEMPTED 1
#define SRQ_BORROWING 2


#define	RQ_NQS		(64)		/* Number of run queues. */
#define	RQ_PPQ		(4)		/* Priorities per queue. */

#define	PRIO_MIN	-20
#define	PRIO_MAX	20
#define	SCHED_PRI_NRESV		(PRIO_MAX - PRIO_MIN)
#define	SCHED_PRI_NHALF		(SCHED_PRI_NRESV / 2)
#define	SCHED_PRI_MIN		(PRI_MIN_BATCH + SCHED_PRI_NHALF)

#define	PRI_MIN_TIMESHARE	(88)
#define	PRI_MAX_TIMESHARE	(PRI_MIN_IDLE - 1)
#define	PRI_MIN_IDLE		(224)

#define	PRI_TIMESHARE_RANGE	(PRI_MAX_TIMESHARE - PRI_MIN_TIMESHARE + 1)
#define	PRI_INTERACT_RANGE	((PRI_TIMESHARE_RANGE - SCHED_PRI_NRESV) / 2)
#define	PRI_BATCH_RANGE		(PRI_TIMESHARE_RANGE - PRI_INTERACT_RANGE)

#define	PRI_MIN_INTERACT	PRI_MIN_TIMESHARE
#define	PRI_MAX_INTERACT	(PRI_MIN_TIMESHARE + PRI_INTERACT_RANGE - 1)
#define	PRI_MIN_BATCH		(PRI_MIN_TIMESHARE + PRI_INTERACT_RANGE)
#define	PRI_MAX_BATCH		PRI_MAX_TIMESHARE

#define	SCHED_INTERACT_MAX	(100)
#define	SCHED_INTERACT_HALF	(SCHED_INTERACT_MAX / 2)
#define	SCHED_INTERACT_THRESH	(30)
#define	SCHED_INTERACT	SCHED_INTERACT_THRESH
#define	PRI_MAX			(255)		/* Lowest priority. */

#define	PRI_MAX_IDLE		(PRI_MAX)

static threadqueue_t* m_run_queues;

__attribute__((const))
INTERRUPT static threadqueue_t* get_run_queue() {
    return &m_run_queues[get_cpu_id()];
}

__attribute__((const))
INTERRUPT static threadqueue_t* get_run_queue_of(int cpu_id) {
    ASSERT(cpu_id < get_cpu_count());
    return &m_run_queues[cpu_id];
}

#define TDS_RUNQ 1

#define	TD_SET_STATE(td, s)	atomic_store((_Atomic(uint32_t)*) (&(td)->state), s)
#define	TD_SET_RUNQ(td)		TD_SET_STATE(td, TDS_RUNQ)

bool runq_head_empty(runq_head_t* rqh) {
    return rqh->first == NULL;
}

void runq_head_insh(runq_head_t* rqh, thread_t* td) {
    if ((td->next_in_bucket = rqh->first) != NULL) {
        rqh->first->prev_in_bucket = &td->next_in_bucket;
    } else {
        rqh->last = &td->next_in_bucket;
    }
    rqh->first = td;
    td->prev_in_bucket = &rqh->first;
}
void runq_head_inst(runq_head_t* rqh, thread_t* td) {
    td->next_in_bucket = NULL;
    td->prev_in_bucket = rqh->last;
    *rqh->last = td;
    rqh->last = &td->next_in_bucket;
}
void runq_head_remove(runq_head_t* rqh, thread_t* td) {
    if (td->next_in_bucket != NULL) {
        td->next_in_bucket->prev_in_bucket = td->prev_in_bucket;
    } else {
        rqh->last = td->prev_in_bucket;
    }
    *td->prev_in_bucket = td->next_in_bucket;
}

static void runq_clrbit(runq_t *rq, int pri)
{
	rq->bits &= ~(1ul << pri);
}

static void runq_setbit(runq_t *rq, int pri)
{
	rq->bits |= (1ul << pri);
}


void runq_add(runq_t* rq, thread_t* td, int flags)
{
	int pri = td->priority / RQ_PPQ;
	td->rqindex = pri;
	runq_setbit(rq, pri);
	runq_head_t* rqh = &rq->queues[pri];
	//CTR4(KTR_RUNQ, "runq_add: td=%p pri=%d %d rqh=%p",
	//    td, td->td_priority, pri, rqh);
	if (flags & SRQ_PREEMPTED) {
		runq_head_insh(rqh, td);
	} else {
		runq_head_inst(rqh, td);
	}
}

void runq_add_pri(runq_t* rq, thread_t* td, uint8_t pri, int flags)
{
	ASSERT(pri < RQ_NQS); // ("runq_add_pri: %d out of range", pri));
	td->rqindex = pri;
	runq_setbit(rq, pri);
	runq_head_t* rqh = &rq->queues[pri];
	//CTR4(KTR_RUNQ, "runq_add_pri: td=%p pri=%d idx=%d rqh=%p",
	//    td, td->td_priority, pri, rqh);
	if (flags & SRQ_PREEMPTED) {
		runq_head_insh(rqh, td);
	} else {
		runq_head_inst(rqh, td);
	}
}


void runq_remove_idx(runq_t* rq, thread_t* td, uint8_t *idx)
{
	//KASSERT(td->td_flags & TDF_INMEM,
	//	("runq_remove_idx: thread swapped out"));
	uint8_t pri = td->rqindex;
    ASSERT(pri < RQ_NQS); // , ("runq_remove_idx: Invalid index %d\n", pri));
	runq_head_t* rqh = &rq->queues[pri];
	//CTR4(KTR_RUNQ, "runq_remove_idx: td=%p, pri=%d %d rqh=%p",
	//    td, td->td_priority, pri, rqh);
	runq_head_remove(rqh, td);
	if (runq_head_empty(rqh)) {
		//CTR0(KTR_RUNQ, "runq_remove_idx: empty");
		runq_clrbit(rq, pri);
		if (idx != NULL && *idx == pri)
			*idx = (pri + 1) % RQ_NQS;
	}
}

void runq_remove(struct runq *rq, struct thread *td)
{
	runq_remove_idx(rq, td, NULL);
}


static void tdq_runq_add(threadqueue_t* rq, thread_t* td, int flags)
{
	//TDQ_LOCK_ASSERT(tdq, MA_OWNED);
	//THREAD_LOCK_BLOCKED_ASSERT(td, MA_OWNED);

	uint8_t pri = td->priority;
	TD_SET_RUNQ(td);

	if (pri < PRI_MIN_BATCH) {
		td->rq = &rq->realtime;
	} else if (pri <= PRI_MAX_BATCH) {
		td->rq = &rq->timeshare;
		/*
		 * This queue contains only priorities between MIN and MAX
		 * batch.  Use the whole queue to represent these values.
		 */
        ASSERT(pri <= PRI_MAX_BATCH && pri >= PRI_MIN_BATCH); //	("Invalid priority %d on timeshare runq", pri));
		if ((flags & (SRQ_BORROWING|SRQ_PREEMPTED)) == 0) {
			pri = RQ_NQS * (pri - PRI_MIN_BATCH) / PRI_BATCH_RANGE;
			pri = (pri + rq->idx) % RQ_NQS;
			/*
			 * This effectively shortens the queue by one so we
			 * can have a one slot difference between idx and
			 * ridx while we wait for threads to drain.
			 */
			if (rq->ridx != rq->idx && pri == rq->ridx)
				pri = (uint8_t)(pri - 1) % RQ_NQS;
		} else {
			pri = rq->ridx;
        }

        runq_add_pri(td->rq, td, pri, flags);
		return;
	} else {
		td->rq = &rq->idle;
    }
    runq_add(td->rq, td, flags);
}

static int
runq_findbit(runq_t *rq)
{
	// TODO: optimize
    for (int i = 0; i < 64; i++) {
        int idx = i;
        if (rq->bits & (1ul << idx)) return idx;
    }
    return -1;
}

static int
runq_findbit_from(runq_t *rq, uint8_t pri)
{
	// TODO: optimize
    for (int i = 0; i < 64; i++) {
        int idx = (i + pri) % 64;
        if (rq->bits & (1ul << idx)) return idx;
    }
    return -1;
}


thread_t*
runq_choose(runq_t *rq)
{
	int pri;
	while ((pri = runq_findbit(rq)) != -1) {
		runq_head_t* rqh = &rq->queues[pri];
		thread_t* td = rqh->first;
		ASSERT(td != NULL); // ("runq_choose: no thread on busy queue"));
		//CTR3(KTR_RUNQ,
		//    "runq_choose: pri=%d thread=%p rqh=%p", pri, td, rqh);
		return (td);
	}
	//CTR1(KTR_RUNQ, "runq_choose: idlethread pri=%d", pri);

	return (NULL);
}

thread_t*
runq_choose_from(runq_t *rq, uint8_t idx)
{
	int pri;
	if ((pri = runq_findbit_from(rq, idx)) != -1) {
		runq_head_t* rqh = &rq->queues[pri];
		thread_t* td = rqh->first;
		ASSERT(td != NULL); //, ("runq_choose: no thread on busy queue"));
		//CTR4(KTR_RUNQ,
		//    "runq_choose_from: pri=%d thread=%p idx=%d rqh=%p",
		//    pri, td, td->td_rqindex, rqh);
		return (td);
	}
	//CTR1(KTR_RUNQ, "runq_choose_from: idlethread pri=%d", pri);

	return (NULL);
}

static void tdq_runq_rem(threadqueue_t* rq, thread_t* td)
{
	//TDQ_LOCK_ASSERT(tdq, MA_OWNED);
	//THREAD_LOCK_BLOCKED_ASSERT(td, MA_OWNED);
	ASSERT(td->rq != NULL);
	//    ("tdq_runq_remove: thread %p null ts_runq", td));
	
    /*if (ts->ts_flags & TSF_XFERABLE) {
		tdq->tdq_transferable--;
		ts->ts_flags &= ~TSF_XFERABLE;
	}*/
	if (td->rq == &rq->timeshare) {
		if (rq->idx != rq->ridx)
			runq_remove_idx(td->rq, td, &rq->ridx);
		else
			runq_remove_idx(td->rq, td, NULL);
	} else
		runq_remove(td->rq, td);
}


/*
 * Load is maintained for all threads RUNNING and ON_RUNQ.  Add the load
 * for this thread to the referenced thread queue.
 */
static void tdq_load_add(threadqueue_t* rq, thread_t* td)
{

	//TDQ_LOCK_ASSERT(tdq, MA_OWNED);
	//THREAD_LOCK_BLOCKED_ASSERT(td, MA_OWNED);
	rq->load++;
	//if ((td->flags & TDF_NOLOAD) == 0)
	//	rq->sysload++;
	//KTR_COUNTER0(KTR_SCHED, "load", tdq->tdq_loadname, tdq->tdq_load);
	//SDT_PROBE2(sched, , , load__change, (int)TDQ_ID(tdq), tdq->tdq_load);
}

/*
 * Remove the load from a thread that is transitioning to a sleep state or
 * exiting.
 */
static void
tdq_load_rem(threadqueue_t* rq, thread_t* td)
{

	//TDQ_LOCK_ASSERT(tdq, MA_OWNED);
	//THREAD_LOCK_BLOCKED_ASSERT(td, MA_OWNED);
	//KASSERT(tdq->tdq_load != 0,
	//    ("tdq_load_rem: Removing with 0 load on queue %d", TDQ_ID(tdq)));

	rq->load--;
	//if ((td->td_flags & TDF_NOLOAD) == 0)
	//	tdq->tdq_sysload--;
	//KTR_COUNTER0(KTR_SCHED, "load", tdq->tdq_loadname, tdq->tdq_load);
	//SDT_PROBE2(sched, , , load__change, (int)TDQ_ID(tdq), tdq->tdq_load);
}

static int
tdq_add(threadqueue_t* rq, thread_t* td, int flags)
{
	//TDQ_LOCK_ASSERT(tdq, MA_OWNED);
	//THREAD_LOCK_BLOCKED_ASSERT(td, MA_OWNED);
	//KASSERT((td->td_inhibitors == 0),
	//    ("sched_add: trying to run inhibited thread"));
	//KASSERT((TD_CAN_RUN(td) || TD_IS_RUNNING(td)),
	//    ("sched_add: bad thread state"));
	//KASSERT(td->td_flags & TDF_INMEM,
	//    ("sched_add: thread swapped out"));

	int lowpri = rq->lowpri;
	if (td->priority < lowpri)
		rq->lowpri = td->priority;
	tdq_runq_add(rq, td, flags);
	tdq_load_add(rq, td);
	return lowpri;
}

static int
sched_interact_score(struct thread *td)
{
	int div;

	/*
	 * The score is only needed if this is likely to be an interactive
	 * task.  Don't go through the expense of computing it if there's
	 * no chance.
	 */
	if (SCHED_INTERACT <= SCHED_INTERACT_HALF &&
		td->runtime >= td->sleeptime)
			return SCHED_INTERACT_HALF;

	if (td->runtime > td->sleeptime) {
		div = MAX(1, td->runtime / SCHED_INTERACT_HALF);
		return (SCHED_INTERACT_HALF +
		    (SCHED_INTERACT_HALF - (td->sleeptime / div)));
	}
	if (td->sleeptime > td->runtime) {
		div = MAX(1, td->sleeptime / SCHED_INTERACT_HALF);
		return (td->runtime / div);
	}
	/* runtime == sleeptime */
	if (td->runtime)
		return (SCHED_INTERACT_HALF);

	/*
	 * This can happen if sleeptime and runtime are 0.
	 */
	return (0);

}
void sched_priority(thread_t* td) {
    // TODO: add nice
    uint8_t score = MAX(0, sched_interact_score(td));
    uint8_t pri;
    if (score < SCHED_INTERACT) {
        pri = PRI_MIN_INTERACT;
        pri += (PRI_MAX_INTERACT - PRI_MIN_INTERACT + 1) * score / SCHED_INTERACT;
        ASSERT(pri >= PRI_MIN_INTERACT && pri <= PRI_MAX_INTERACT);
    } else {
        pri = SCHED_PRI_MIN;
        // TODO: timeshare priority
        ASSERT(pri >= PRI_MIN_BATCH && pri <= PRI_MAX_BATCH);
    }
    //sched_user_prio(td, pri);
}


static int
sched_pickcpu(struct thread *td, int flags)
{
	return 0;
}

static threadqueue_t *
sched_setcpu(struct thread *td, int cpu, int flags)
{

	//runqueue_t* rq;
	//struct mtx *mtx;

	//THREAD_LOCK_ASSERT(td, MA_OWNED);
	threadqueue_t* rq = get_run_queue_of(cpu);
	td->cpu = cpu;
	/*
	 * If the lock matches just return the queue.
	 */
	if (td->lock == &rq->lock) {
		//KASSERT((flags & SRQ_HOLD) == 0,
		//    ("sched_setcpu: Invalid lock for SRQ_HOLD"));
		return rq;
	}


    // TODO:
    // FIXME:
	return rq;
    /*
	 * The hard case, migration, we need to block the thread first to
	 * prevent order reversals with other cpus locks.
	 */
	/*spinlock_enter();
	mtx = thread_lock_block(td);
	if ((flags & SRQ_HOLD) == 0)
		mtx_unlock_spin(mtx);
	TDQ_LOCK(tdq);
	thread_lock_unblock(td, TDQ_LOCKPTR(tdq));
	spinlock_exit();
	return (tdq);*/
}

static inline int
sched_shouldpreempt(int pri, int cpri, int remote)
{
	if (pri >= cpri)
		return 0;
	return 1;
}

static void
tdq_notify(threadqueue_t *rq, int lowpri)
{
	int cpu;

	//TDQ_LOCK_ASSERT(tdq, MA_OWNED);
	ASSERT(rq->lowpri <= lowpri);
	//    ("tdq_notify: lowpri %d > tdq_lowpri %d", lowpri, tdq->tdq_lowpri));

	if (rq->owepreempt)
		return;

	/*
	 * Check to see if the newly added thread should preempt the one
	 * currently running.
	 */
	if (!sched_shouldpreempt(rq->lowpri, lowpri, 1))
		return;

	/*
	 * Make sure that our caller's earlier update to tdq_load is
	 * globally visible before we read tdq_cpu_idle.  Idle thread
	 * accesses both of them without locks, and the order is important.
	 */
	atomic_thread_fence(memory_order_seq_cst);

	cpu = rq->idx;

	/*
	 * The run queues have been updated, so any switch on the remote CPU
	 * will satisfy the preemption request.
	 */
	rq->owepreempt = 1;

    // TODO: wakeup
	lapic_send_ipi(IRQ_PREEMPT, cpu);
}


void
sched_add(thread_t *td, int flags)
{
	int cpu, lowpri;

	//KTR_STATE2(KTR_SCHED, "thread", sched_tdname(td), "runq add",
	//    "prio:%d", td->td_priority, KTR_ATTR_LINKED,
	//    sched_tdname(curthread));
	//KTR_POINT1(KTR_SCHED, "thread", sched_tdname(curthread), "wokeup",
	//    KTR_ATTR_LINKED, sched_tdname(td));
	//SDT_PROBE4(sched, , , enqueue, td, td->td_proc, NULL, 
	//    flags & SRQ_PREEMPTED);
	
    //THREAD_LOCK_ASSERT(td, MA_OWNED);

	/*
	 * Recalculate the priority before we select the target cpu or
	 * run-queue.
	 */
	sched_priority(td);
	/*
	 * Pick the destination cpu and if it isn't ours transfer to the
	 * target cpu.
	 */
	cpu = sched_pickcpu(td, flags);
	threadqueue_t* tdq = sched_setcpu(td, cpu, flags);
	lowpri = tdq_add(tdq, td, flags);
	if (cpu != get_cpu_id())
		tdq_notify(tdq, lowpri);
	
    //else if (!(flags & SRQ_YIELDING))
	//	sched_setpreempt(td->td_priority);
//
	//if (!(flags & SRQ_HOLDTD))
	//	thread_unlock(td);
}


// calculate an interactivity score (0 = most interactive, SCORE_RANGE_MAX = least interactive)
// it uses a rough approximation of sigmoid function (except this one touches the origin!)
// the use of such a function compresses the extremes
// so a process that runs 1% of the time is put in the same bucket as one that runs 0.1%,
// as the differences at the very high and very low end don't matter as much  




bool scheduler_can_spin(int i) {
    // don't spin anymore...
    if (i > 4) return false;

    // single core machine, never spin
    if (get_cpu_count() <= 1) return false;

    // All cpus are doing work, so we might need to
    // do work as well
    if (m_idle_cpus_count == 0) return false;

    // we have stuff to run on our local run queue
    //if (!run_(get_cpu_id())) return false;
    return false;
    // we can spin a little :)
    //return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Wake a thread
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void scheduler_ready_thread(thread_t* thread) {
    thread->sched_link = NULL;

    scheduler_preempt_disable();

    ASSERT((get_thread_status(thread) & ~THREAD_SUSPEND) == THREAD_STATUS_WAITING);

    // Mark as runnable
    cas_thread_state(thread, THREAD_STATUS_WAITING, THREAD_STATUS_RUNNABLE);

    // Put in the run queue
    int flags = 0;
    bool is_still_in_timeslice = (microtime() - thread->current_run_start) < (10 * 1000);
    if (is_still_in_timeslice) flags |= SRQ_PREEMPTED; 
    sched_add(thread, flags);

    // in case someone can steal
    wake_cpu();

    scheduler_preempt_enable();
}

INTERRUPT static bool cas_from_preempted(thread_t* thread) {
    thread_status_t old = THREAD_STATUS_PREEMPTED;
    return atomic_compare_exchange_strong(&thread->status, &old, THREAD_STATUS_WAITING);
}

INTERRUPT static bool cas_to_suspend(thread_t* thread, thread_status_t old, thread_status_t new) {
    ASSERT(new == (old | THREAD_SUSPEND));
    return atomic_compare_exchange_strong(&thread->status, &old, new);
}

INTERRUPT static void cas_from_suspend(thread_t* thread, thread_status_t old, thread_status_t new) {
    bool success = false;
    if (new == (old & ~THREAD_SUSPEND)) {
        if (atomic_compare_exchange_strong(&thread->status, &old, new)) {
            success = true;
        }
    }
    ASSERT(success);
}

suspend_state_t scheduler_suspend_thread(thread_t* thread) {
    bool stopped = false;

    for (int i = 0;; i++) {
        thread_status_t status = get_thread_status(thread);
        switch (status) {
            default:
                // the thread is already suspended, make sure of it
                ASSERT(status & THREAD_SUSPEND);
                break;

            case THREAD_STATUS_DEAD:
                // nothing to suspend.
                return (suspend_state_t){ .dead = true };

            case THREAD_STATUS_PREEMPTED:
                // We (or someone else) suspended the thread. Claim
                // ownership of it by transitioning it to
                // THREAD_STATUS_WAITING
                if (!cas_from_preempted(thread)) {
                    break;
                }

                // Clear the preemption request.
                thread->preempt_stop = false;

                // We stopped the thread, so we have to ready it later
                stopped = true;

                status = THREAD_STATUS_WAITING;

                // fallthrough

            case THREAD_STATUS_RUNNABLE:
            case THREAD_STATUS_WAITING:
                // Claim goroutine by setting suspend bit.
                // This may race with execution or readying of thread.
                // The scan bit keeps it from transition state.
                if (!cas_to_suspend(thread, status, status | THREAD_SUSPEND)) {
                    break;
                }

                // Clear the preemption request.
                thread->preempt_stop = false;

                // The thread is already at a safe-point
                // and we've now locked that in.
                return (suspend_state_t) { .thread = thread, .stopped = stopped };

            case THREAD_STATUS_RUNNING:
                // Optimization: if there is already a pending preemption request
                // (from the previous loop iteration), don't bother with the atomics.
                if (thread->preempt_stop) {
                    break;
                }

                // Temporarily block state transitions.
                if (!cas_to_suspend(thread, THREAD_STATUS_RUNNING, THREAD_STATUS_RUNNING | THREAD_SUSPEND)) {
                    break;
                }

                // Request preemption
                thread->preempt_stop = true;

                // Prepare for asynchronous preemption.
                cas_from_suspend(thread, THREAD_STATUS_RUNNING | THREAD_SUSPEND, THREAD_STATUS_RUNNING);

                // Send asynchronous preemption. We do this
                // after CASing the thread back to THREAD_STATUS_RUNNING
                // because preempt may be synchronous and we
                // don't want to catch the thread just spinning on
                // its status.
                // TODO: scheduler_preempt(thread); or something
        }

        for (int x = 0; x < 10; x++) {
            __builtin_ia32_pause();
        }
    }
}

void scheduler_resume_thread(suspend_state_t state) {
    if (state.dead) {
        return;
    }

    // switch back to non-suspend state
    thread_status_t status = get_thread_status(state.thread);
    cas_from_suspend(state.thread, status, status & ~THREAD_SUSPEND);

    if (state.stopped) {
        // We stopped it, so we need to re-schedule it.
        scheduler_ready_thread(state.thread);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Preemption
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static size_t CPU_LOCAL m_preempt_disable_depth;

void scheduler_preempt_disable(void) {
    if (m_preempt_disable_depth++ == 0) {
        __writecr8(PRIORITY_NO_PREEMPT);
    }
}

void scheduler_preempt_enable(void) {
    if (--m_preempt_disable_depth == 0) {
        __writecr8(PRIORITY_NORMAL);
    }
}

bool scheduler_is_preemption(void) {
    return m_preempt_disable_depth == 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actual scheduling
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * The currently running thread on the thread
 */
static thread_t* CPU_LOCAL m_current_thread;

/**
 * Ticks of the scheduler
 */
static int32_t CPU_LOCAL m_scheduler_tick;

//----------------------------------------------------------------------------------------------------------------------
// Actually running a thread
//----------------------------------------------------------------------------------------------------------------------

static void scheduler_set_deadline() {
    lapic_set_timeout(10 * 1000);
}

static void scheduler_cancel_deadline() {
    lapic_set_deadline(0);
}

static void validate_context(interrupt_context_t* ctx) {
    ASSERT(STACK_POOL_START <= ctx->rsp && ctx->rsp < STACK_POOL_END);
}

/**
 * Execute the thread on the current cpu
 *
 * @param ctx               [IN] The context of the scheduler interrupt
 * @param thread            [IN] The thread to run
 */
INTERRUPT static void execute(interrupt_context_t* ctx, thread_t* thread) {
    // set the current thread
    m_current_thread = thread;

    // get ready to run it
    cas_thread_state(thread, THREAD_STATUS_RUNNABLE, THREAD_STATUS_RUNNING);

    // add another tick
    m_scheduler_tick++;

    // set a new timeslice of 10 milliseconds
    scheduler_set_deadline();

    // set the gprs context
    restore_thread_context(thread, ctx);
    validate_context(ctx);
}

static void save_current_thread(interrupt_context_t* ctx, bool park) {
    ASSERT(m_current_thread != NULL);
    thread_t* current_thread = m_current_thread;
    m_current_thread = NULL;

    // save the state and set the thread to runnable
    validate_context(ctx);
    save_thread_context(current_thread, ctx);

    // put the thread back
    if (!park) {
        // put the thread on the global run queue
        if (current_thread->preempt_stop) {
            // set as preempted, don't add back to queue
            cas_thread_state(current_thread, THREAD_STATUS_RUNNING, THREAD_STATUS_PREEMPTED);

        } else {
            // set the thread to be runnable
            cas_thread_state(current_thread, THREAD_STATUS_RUNNING, THREAD_STATUS_RUNNABLE);

            // put in the local run queue
            sched_add(current_thread, 0);
        }
    } else {
        cas_thread_state(current_thread, THREAD_STATUS_RUNNING, THREAD_STATUS_WAITING);
    }
}


//----------------------------------------------------------------------------------------------------------------------
// random order for randomizing work stealing
//----------------------------------------------------------------------------------------------------------------------

typedef struct random_enum {
    uint32_t i;
    uint32_t count;
    uint32_t pos;
    uint32_t inc;
} random_enum_t;

static uint32_t m_random_order_count = 0;
static uint32_t* m_random_order_coprimes = NULL;

static uint32_t gcd(uint32_t a, uint32_t b) {
    while (b != 0) {
        uint32_t t = b;
        b = a % b;
        a = t;
    }
    return a;
}

static void random_order_reset(int count) {
    m_random_order_count = count;
    arrsetlen(m_random_order_coprimes, 0);
    for (int i = 1; i <= count; i++) {
        if (gcd(i, count) == 1) {
            arrpush(m_random_order_coprimes, i);
        }
    }
}

static random_enum_t random_order_start(uint32_t i) {
    return (random_enum_t) {
        .count = m_random_order_count,
        .pos = i % m_random_order_count,
        .inc = m_random_order_coprimes[i / m_random_order_count % arrlen(m_random_order_coprimes)]
    };
}

static bool random_enum_done(random_enum_t* e) {
    return e->i == e->count;
}

static void random_enum_next(random_enum_t* e) {
    e->i++;
    e->pos = (e->pos + e->inc) % e->count;
}

static uint32_t random_enum_position(random_enum_t* e) {
    return e->pos;
}



/**
 * Interrupts the poller
 */
static void break_poller() {
    int poller = atomic_load(&m_polling_cpu);
    if (poller != -1) {
        lapic_send_ipi(IRQ_WAKEUP, poller);
    }
}

void scheduler_wake_poller(int64_t when) {
    if (atomic_load(&m_last_poll) == 0) {
        // In find_runnable we ensure that when polling the poll_until
        // field is either zero or the time to which the current poll
        // is expected to run. This can have a spurious wakeup
        // but should never miss a wakeup.
        int64_t poller_wake_until = atomic_load(&m_poll_until);
        if (poller_wake_until == 0 || poller_wake_until > when) {
            break_poller();
        }
    } else {
        // There are no threads in the poller, try to
        // one there so it can handle new timers
        wake_cpu();
    }
}

static void cpu_put_idle() {
    // clear if there are no timers
    update_cpu_timers_mask();
    mask_set(m_idle_cpus, get_cpu_id());
    atomic_fetch_add(&m_idle_cpus_count, 1);
}

static void cpu_wake_idle() {
    set_has_timers(get_cpu_id());
    mask_clear(m_idle_cpus, get_cpu_id());
    atomic_fetch_sub(&m_idle_cpus_count, 1);
}

static thread_t*
tdq_choose(threadqueue_t* rq)
{
	//TDQ_LOCK_ASSERT(tdq, MA_OWNED);
	thread_t* td = runq_choose(&rq->realtime);
	if (td != NULL)
		return (td);
	td = runq_choose_from(&rq->timeshare, rq->ridx);
	if (td != NULL) {
		ASSERT(td->priority >= PRI_MIN_BATCH);
		    //("tdq_choose: Invalid priority on timeshare queue %d",
		    //td->priority));
		return (td);
	}
	td = runq_choose(&rq->idle);
	if (td != NULL) {
		ASSERT(td->priority >= PRI_MIN_IDLE);
		    //("tdq_choose: Invalid priority on idle queue %d",
		    //td->td_priority));
		return (td);
	}

	return (NULL);
}


// FIXME:
thread_t*
sched_choose(void)
{
	threadqueue_t* rq = get_run_queue();
	//TDQ_LOCK_ASSERT(tdq, MA_OWNED);
	thread_t* td = tdq_choose(rq);
	if (td != NULL) {
		tdq_runq_rem(rq, td);
		rq->lowpri = td->priority;
	} else { 
		rq->lowpri = PRI_MAX_IDLE;
		return NULL;
        // TODO:
        //td = PCPU_GET(idlethread);
	}
	//rq->curthread = td;
	return td;
}


static thread_t* find_runnable() {
    thread_t* thread = NULL;

    while (true) {
        // now and poll_until are saved for work stealing later,
        // which may steal timers. It's important that between now
        // and then, nothing blocks, so these numbers remain mostly
        // relevant.
        int64_t now = 0;
        int64_t poll_until = 0;
        check_timers(get_cpu_id(), &now, &poll_until, NULL);

        // TODO: try to schedule gc worker (do we care about doing it explicitly?)

        // get from the local run queue
        thread = sched_choose();
        if (thread != NULL) {
            return thread;
        }

        //
        // We have nothing to do
        //

        // prepare to enter idle
        lock_scheduler();

        // we are now idle
        cpu_put_idle();

        unlock_scheduler();

        // restore the spinning since we are no longer spinning
        bool was_spinning = m_spinning;
        if (m_spinning) {
            m_spinning = false;
            ASSERT (atomic_fetch_sub(&m_number_spinning, 1) > 0);

            //
            // check for timer creation or expiry concurrently with
            // transition from spinning to non-spinning
            //
            for (int cpu = 0; cpu < get_cpu_count(); cpu++) {
                if (cpu_has_timers(cpu)) {
                    int64_t when = nobarrier_wake_time(cpu);
                    if (when != 0 && (poll_until == 0 || when < poll_until)) {
                        poll_until = when;
                    }
                }
            }
        }

        // Poll until next timer
        if (poll_until != 0 && atomic_exchange(&m_last_poll, 0) != 0) {
            atomic_store(&m_poll_until, poll_until);

            ASSERT(!m_spinning);

            // refresh now
            now = microtime();
            int64_t delay = -1;
            if (poll_until != 0) {
                delay = poll_until - now;
                if (delay < 0) {
                    delay = 0;
                }
            }

            // TODO: we don't have a polling system yet
            // decide how much to sleep
            int32_t wait_us;
            if (delay < 0) {
                // block indefinitely
                wait_us = -1;
            } else if (delay == 0) {
                // no blocking
                // TODO: not really helping
                wait_us = 0;
            } else if (delay < 1000) {
                // the delay is smaller than 1ms, round up
                wait_us = 1;
            } else if (delay < 1000000000000) {
                // turn the
                wait_us = (int32_t)(delay / 1000);
            } else {
                // An arbitrary cap on how long to wait for a timer.
                // 1e9 ms == ~11.5 days
                wait_us = 1000000000;
            }

            // TODO: what we wanna do in reality is have a poll
            //       that has a timeout of wait_us, but for now
            //       we are just gonna sleep for that time
            if (wait_us > 0) {
                // we want to sleep, so sleep for the given amount of
                // time or when an interrupt happens, we will keep at
                // a normal priority because we want to be waiting for
                // the timer if someone else can process the irq
                atomic_store(&m_polling_cpu, get_cpu_id());

                // prepare to sleep, we want to get a wakeup which does not
                // cause a stack change, and we want it after the given amount of
                // time
                lapic_set_wakeup();
                lapic_set_timeout(wait_us);

                m_waiting_for_irq = true;
                __asm__ ("sti; hlt; cli");
                m_waiting_for_irq = false;

                // we are back, return to have the timer be preempt, and also
                // cancel the deadline in the case that we got an early wakeup
                // which was no related to the timer
                lapic_set_preempt();
                scheduler_cancel_deadline();

                atomic_store(&m_polling_cpu, -1);
            }

            atomic_store(&m_poll_until, 0);
            atomic_store(&m_last_poll, now);

            // remove from idle cpus since we might have work to do
            lock_scheduler();
            cpu_wake_idle();
            unlock_scheduler();

            // we are spinning
            if (was_spinning) {
                m_spinning = true;
                atomic_fetch_add(&m_number_spinning, 1);
            }

            // we might have work, go to the top
            continue;
        } else if (poll_until != 0) {
            int64_t poller_poll_until = atomic_load(&m_poll_until);
            if (poller_poll_until == 0 || poller_poll_until > poll_until) {
                break_poller();
            }
        }

        // we have nothing to do, so put the cpu into
        // a sleeping state until an interrupt or something
        // else happens. we will lower the state to make sure
        // that interrupts will arrive to us at the highest
        // priority
        __writecr8(PRIORITY_SCHEDULER_WAIT);
        m_waiting_for_irq = true;
        __asm__ ("sti; hlt; cli");
        m_waiting_for_irq = false;
        __writecr8(PRIORITY_NORMAL);

        // we might have work so wake the cpu
        lock_scheduler();
        cpu_wake_idle();
        unlock_scheduler();
    }
}

INTERRUPT static void schedule() {
    // will block until a thread is ready, essentially an idle loop,
    // this must return something eventually.
    thread_t* thread = find_runnable();

    if (m_spinning) {
        m_spinning = false;
        atomic_fetch_sub(&m_number_spinning, 1);
        wake_cpu();
    }

    // actually run the new thread
    interrupt_context_t ctx = {};
    execute(&ctx, thread);

    // finally restore from the interrupt context, jumping
    // directly to the process
    restore_interrupt_context(&ctx);
}

/**
 * The idle task
 */
static void* CPU_LOCAL m_idle_stack;

/**
 * This sets a dummy schedule threads
 */
INTERRUPT static void set_schedule_thread(interrupt_context_t* ctx) {
    // set the stack and rip
    memset(ctx, 0, sizeof(*ctx));
    ctx->rsp = (uint64_t) m_idle_stack + SIZE_8KB;
    ctx->rip = (uint64_t) schedule;
    ctx->rflags = (rflags_t) { .always_one = 1 };
    ctx->cs = GDT_CODE;
    ctx->ss = GDT_DATA;

    // push a null to the stack, making sure it returns
    // to nothing
    PUSH(uint64_t, ctx->rsp, 0);
    PUSH(uint64_t, ctx->rsp, 0);
}

//----------------------------------------------------------------------------------------------------------------------
// Scheduler callbacks
//----------------------------------------------------------------------------------------------------------------------

static void verify_can_enter_scheduler() {
    // NOTE: PRIORITY_NORMAL is accepted, but also PRIORITY_SCHEDULER_WAIT
    ASSERT(__readcr8() <= PRIORITY_NORMAL);
}

INTERRUPT void scheduler_on_schedule(interrupt_context_t* ctx) {
    if (m_waiting_for_irq) {
        // we got a spurious preempt, don't do anything
        // NOTE: we are right now running with the same stack as
        //       the schedule below us, so we need to be super careful
        //       to not change anything of significance between the callpath
        //       we have underneath us and our code
        return;
    }

    verify_can_enter_scheduler();

    // save the current thread, don't park it
    save_current_thread(ctx, false);

    // TODO: run the load balancer

    // advance the insert index
    threadqueue_t *rq = get_run_queue();
    if (rq->idx == rq->ridx) {
        rq->idx = (rq->idx + 1) % 16;
        if (rq->timeshare.queues[rq->ridx].first == NULL) rq->ridx = rq->idx;
    }

    // now schedule a new thread
    set_schedule_thread(ctx);
}

INTERRUPT void scheduler_on_park(interrupt_context_t* ctx) {
    verify_can_enter_scheduler();

    // save the current thread, park it
    save_current_thread(ctx, true);

    // check if we need to call a callback before we schedule
    if (ctx->rdi != 0) {
        ((void(*)(uint64_t))ctx->rdi)(ctx->rsi);
    }

    // cancel the deadline of the current thread, as it is parked
    scheduler_cancel_deadline();

    // schedule a new thread
    set_schedule_thread(ctx);
}

INTERRUPT void scheduler_on_drop(interrupt_context_t* ctx) {
    verify_can_enter_scheduler();

    thread_t* current_thread = get_current_thread();
    m_current_thread = NULL;

    if (current_thread != NULL) {
        // change the status to dead
        cas_thread_state(current_thread, THREAD_STATUS_RUNNING, THREAD_STATUS_DEAD);

        // don't keep the managed thread alive anymore
        current_thread->tcb->managed_thread = NULL;

        // release the reference that the scheduler has
        release_thread(current_thread);
    }

    // cancel the deadline of the current thread, as it is dead
    scheduler_cancel_deadline();

    set_schedule_thread(ctx);
}

//----------------------------------------------------------------------------------------------------------------------
// Interrupts to call the scheduler
//----------------------------------------------------------------------------------------------------------------------

void scheduler_yield() {
    // don't preempt if we can't preempt
    if (m_preempt_disable_depth > 0) {
        return;
    }

    __asm__ volatile (
        "int %0"
        :
        : "i"(IRQ_YIELD)
        : "memory");
}

void scheduler_park(void(*callback)(void* arg), void* arg) {
    __asm__ volatile (
        "int %0"
        :
        : "i"(IRQ_PARK)
        , "D"(callback)
        , "S"(arg)
        : "memory");
}

void scheduler_drop_current() {
    __asm__ volatile (
        "int %0"
        :
        : "i"(IRQ_DROP)
        : "memory");
}

void scheduler_startup() {
    // set to normal running priority
    __writecr8(PRIORITY_NORMAL);

    // enable interrupts
    _enable();

    // drop the current thread in favor of starting
    // the scheduler
    scheduler_drop_current();
}

INTERRUPT thread_t* get_current_thread() {
    return m_current_thread;
}

err_t init_scheduler() {
    err_t err = NO_ERROR;

    // initialize our random for the amount of cores we have
    random_order_reset(get_cpu_count());

    // set the last poll
    atomic_store(&m_last_poll, microtime());

    m_run_queues = malloc(get_cpu_count() * sizeof(threadqueue_t));
    CHECK(m_run_queues != NULL);
    
    // TODO: not rely on malloc zeroing
    for (int i = 0; i < get_cpu_count(); i++) {
        for (int j = 0; j < 64; j++) {
            m_run_queues[i].realtime.queues[j].first = NULL;
            m_run_queues[i].realtime.queues[j].last = &m_run_queues[i].realtime.queues[j].first;
            m_run_queues[i].timeshare.queues[j].first = NULL;
            m_run_queues[i].timeshare.queues[j].last = &m_run_queues[i].timeshare.queues[j].first;
            m_run_queues[i].idle.queues[j].first = NULL;
            m_run_queues[i].idle.queues[j].last = &m_run_queues[i].idle.queues[j].first;
        }
    }

    // and set the scheduler stack
    CHECK_AND_RETHROW(init_scheduler_per_core());

cleanup:
    return err;
}

err_t init_scheduler_per_core() {
    err_t err = NO_ERROR;

    m_idle_stack = palloc(SIZE_8KB);
    CHECK(m_idle_stack != NULL);

cleanup:
    return err;
}

static void scheduler_do_thing(_Atomic(uint32_t)* x, int p, waitable_t* done) {
    for (int i = 0; i < 3; i++) {
        uint32_t expected = get_cpu_count() * i + p;
        while (atomic_load(x) != expected);
        atomic_store(x, expected + 1);
    }
    waitable_send(done, true);
}

void scheduler_self_test() {
    TRACE("\tScheduler self-test");

    int N = 10;
    int P = get_cpu_count();

    for (int try = 0; try < N; try++) {
        waitable_t* done = create_waitable(0);

        int x = 0;
        for (int p = 0; p < P; p++) {
            thread_t* t = create_thread((void*)scheduler_do_thing, NULL, "test-%d", p);
            t->save_state.rdi = (uintptr_t)&x;
            t->save_state.rsi = p;
            t->save_state.rdx = (uintptr_t)done;
            scheduler_ready_thread(t);
        }

        for (int p = 0; p < P; p++) {
            waitable_wait(done, true);
        }

        release_waitable(done);
    }
}
