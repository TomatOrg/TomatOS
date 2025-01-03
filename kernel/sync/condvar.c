#include "condvar.h"

#include <lib/except.h>

#include "parking_lot.h"

typedef struct notify_ctx {
    mutex_t* mutex;
    condvar_t* condvar;
} notify_ctx_t;

static inline bool mutex_mark_parked_if_locked(mutex_t* mutex) {
    uint8_t state = atomic_load_explicit(&mutex->state, memory_order_relaxed);
    for (;;) {
        if ((state & MUTEX_LOCKED) == 0) {
            return false;
        }

        if (atomic_compare_exchange_weak_explicit(
            &mutex->state,
            &state, state | MUTEX_PARKED,
            memory_order_relaxed, memory_order_relaxed
        )) {
            return true;
        }
    }
}


static inline void mutex_mark_parked(mutex_t* mutex) {
    atomic_fetch_or_explicit(&mutex->state, MUTEX_PARKED, memory_order_relaxed);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Notify one
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static requeue_op_t condvar_notify_one_validate(void* arg) {
    notify_ctx_t* cv = arg;

    // Make sure that our atomic state still points to the same
    // mutex. If not then it means that all threads on the current
    // mutex were woken up and a new waiting thread switched to a
    // different mutex. In that case we can get away with doing
    // nothing.
    if (atomic_load_explicit(&cv->condvar->mutex, memory_order_relaxed) != cv->mutex) {
        return REQUEUE_OP_ABORT;
    }

    // Unpark one thread if the mutex is unlocked, otherwise just
    // requeue everything to the mutex. This is safe to do here
    // since unlocking the mutex when the parked bit is set requires
    // locking the queue. There is the possibility of a race if the
    // mutex gets locked after we check, but that doesn't matter in
    // this case.
    if (mutex_mark_parked_if_locked(cv->mutex)) {
        return REQUEUE_OP_REQUEUE_ONE;
    } else {
        return REQUEUE_OP_UNPARK_ONE;
    }
}

static size_t condvar_notify_one_callback(void* arg, requeue_op_t op, unpark_result_t result) {
    notify_ctx_t* cv = arg;

    // Clear our state if there are no more waiting threads
    if (!result.have_more_threads) {
        atomic_store_explicit(&cv->condvar->mutex, NULL, memory_order_relaxed);
    }
    return 0;
}

__attribute__((cold))
bool condvar_notify_one_slow(condvar_t* condvar, mutex_t* mutex) {
    size_t from = (size_t)condvar;
    size_t to = (size_t)mutex;
    notify_ctx_t ctx = { .condvar = condvar, .mutex = mutex };
    unpark_result_t result = parking_lot_unpark_requeue(
        from, to,
        condvar_notify_one_validate,
        condvar_notify_one_callback,
        &ctx
    );
    return result.unparked_threads + result.requeued_threads != 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Notify all
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static requeue_op_t condvar_notify_all_validate(void* arg) {
    notify_ctx_t* cv = arg;

    // Make sure that our atomic state still points to the same
    // mutex. If not then it means that all threads on the current
    // mutex were woken up and a new waiting thread switched to a
    // different mutex. In that case we can get away with doing
    // nothing.
    if (atomic_load_explicit(&cv->condvar->mutex, memory_order_relaxed) != cv->mutex) {
        return REQUEUE_OP_ABORT;
    }

    // Clear our state since we are going to unpark or requeue all
    // threads.
    atomic_store_explicit(&cv->condvar->mutex, NULL, memory_order_relaxed);


    // Unpark one thread if the mutex is unlocked, otherwise just
    // requeue everything to the mutex. This is safe to do here
    // since unlocking the mutex when the parked bit is set requires
    // locking the queue. There is the possibility of a race if the
    // mutex gets locked after we check, but that doesn't matter in
    // this case.
    if (mutex_mark_parked_if_locked(cv->mutex)) {
        return REQUEUE_OP_REQUEUE_ALL;
    } else {
        return REQUEUE_OP_UNPARK_ONE_REQUEUE_REST;
    }
}

static size_t condvar_notify_all_callback(void* arg, requeue_op_t op, unpark_result_t result) {
    notify_ctx_t* cv = arg;

    // If we requeued threads to the mutex, mark it as having
    // parked threads. The RequeueAll case is already handled above.
    if (op == REQUEUE_OP_UNPARK_ONE_REQUEUE_REST && result.requeued_threads != 0) {
        mutex_mark_parked(cv->mutex);
    }

    return 0;
}

__attribute__((cold))
size_t condvar_notify_all_slow(condvar_t* condvar, mutex_t* mutex) {
    size_t from = (size_t)condvar;
    size_t to = (size_t)mutex;
    notify_ctx_t ctx = { .condvar = condvar, .mutex = mutex };
    unpark_result_t result = parking_lot_unpark_requeue(
        from, to,
        condvar_notify_all_validate,
        condvar_notify_all_callback,
        &ctx
    );
    return result.unparked_threads + result.requeued_threads;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Wait
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct wait_ctx {
    condvar_t* condvar;
    mutex_t* mutex;
    bool bad_mutex;
    bool requeued;
} wait_ctx_t;

static bool condvar_wait_validate(void* arg) {
    wait_ctx_t* ctx = arg;

    // Ensure we don't use two different mutexes with the same
    // Condvar at the same time. This is done while locked to
    // avoid races with notify_one
    mutex_t* state = atomic_load_explicit(&ctx->condvar->mutex, memory_order_relaxed);
    if (state == NULL) {
        atomic_store_explicit(&ctx->condvar->mutex, ctx->mutex, memory_order_relaxed);
    } else if (state != ctx->mutex) {
        ctx->bad_mutex = true;
        return false;
    }

    return true;
}

static void condvar_wait_before_sleep(void* arg) {
    wait_ctx_t* ctx = arg;

    // unlock the mutex before sleeping...
    mutex_unlock(ctx->mutex);
}

static void condvar_wait_timed_out(void* arg, size_t key, bool was_last_thread) {
    wait_ctx_t* ctx = arg;

    // If we were requeued to a mutex, then we did not time out.
    // We'll just park ourselves on the mutex again when we try
    // to lock it later.
    ctx->requeued = key != (size_t)ctx->condvar;

    // If we were the last thread on the queue then we need to
    // clear our state. This is normally done by the
    // notify_{one,all} functions when not timing out.
    if (!ctx->requeued && was_last_thread) {
        atomic_store_explicit(&ctx->condvar->mutex, NULL, memory_order_relaxed);
    }
}

bool condvar_wait_until(condvar_t* condvar, mutex_t* mutex, uint64_t deadline) {
    wait_ctx_t ctx = {
        .bad_mutex = false,
        .requeued = false,
        .mutex = mutex,
        .condvar = condvar,
    };
    park_result_t result = parking_lot_park(
        (size_t)condvar,
        condvar_wait_validate,
        condvar_wait_before_sleep,
        condvar_wait_timed_out,
        &ctx,
        0,
        deadline
    );

    // Panic if we tried to use multiple mutexes with a Condvar. Note
    // that at this point the MutexGuard is still locked. It will be
    // unlocked by the unwinding logic.
    if (ctx.bad_mutex) {
        ASSERT(!"attempted to use a condition variable with more than one mutex");
    }

    // ... and re-lock it once we are done sleeping
    if (result.unpark_token != 1) {
        mutex_lock(mutex);
    }

    bool is_unparked = !result.timed_out && !result.invalid;
    return !(is_unparked || ctx.requeued);
}

