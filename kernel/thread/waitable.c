#include "waitable.h"
#include "scheduler.h"
#include "timer.h"
#include "time/tsc.h"

#include <mem/malloc.h>

waitable_t* create_waitable(size_t size) {
    waitable_t* waitable = malloc(sizeof(waitable_t));
    if (waitable == NULL) {
        return NULL;
    }
    waitable->size = size;
    waitable->lock = INIT_SPINLOCK();
    return waitable;
}

waitable_t* put_waitable(waitable_t* waitable) {
    atomic_fetch_add(&waitable->ref_count, 1);
    return waitable;
}

void release_waitable(waitable_t* waitable) {
    if (atomic_fetch_sub(&waitable->ref_count, 1) == 1) {
        ASSERT(waitable->closed);
        SAFE_FREE(waitable);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void wait_queue_enqueue(wait_queue_t* q, waiting_thread_t* wt) {
    wt->next = NULL;
    waiting_thread_t* x = q->last;
    if (x == NULL) {
        wt->prev = NULL;
        q->first = wt;
        q->last = wt;
    } else {
        wt->prev = x;
        x->next = wt;
        q->last = wt;
    }
}

static waiting_thread_t* wait_queue_dequeue(wait_queue_t* q) {
    while (true) {
        waiting_thread_t* wt = q->first;
        if (wt == NULL) {
            return NULL;
        }

        waiting_thread_t* y = wt->next;
        if (y == NULL) {
            q->first = NULL;
            q->last = NULL;
        } else {
            y->prev = NULL;
            q->first = y;
            wt->next = NULL;
        }

        // if a thread was put on this queue because of a
        // select, there is a small window between the thread
        // being woken up by a different case and it grabbing the
        // waitable locks. Once it has the lock
        // it removes itself from the queue, so we won't see it after that.
        // We use a flag in the thread struct to tell us when someone
        // else has won the race to signal this thread but the thread
        // hasn't removed itself from the queue yet.
        uint32_t zero = 0;
        if (wt->is_select && !atomic_compare_exchange_strong(&wt->thread->select_done, &zero, 1)) {
            continue;
        }

        return wt;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static bool waitable_full(waitable_t* w) {
    if (w->size == 0) {
        return w->wait_queue.first == NULL;
    }
    return w->count == w->size;
}

static bool waitable_empty(waitable_t* w) {
    if (w->size == 0) {
        return w->send_queue.first == NULL;
    }
    return w->count == 0;
}

bool waitable_send(waitable_t* w, bool block) {
    if (!block && !w->closed && waitable_full(w)) {
        return false;
    }

    spinlock_lock(&w->lock);

    if (w->closed) {
        spinlock_unlock(&w->lock);
        return false;
    }

    waiting_thread_t* wt = wait_queue_dequeue(&w->wait_queue);
    if (wt != NULL) {
        // Found a waiting receiver. We pass the value we want to send
        // directly to the receiver
        spinlock_unlock(&w->lock);

        // wake it up
        wt->thread->waker = wt;
        wt->success = true;
        scheduler_ready_thread(wt->thread);

        return true;
    }

    if (w->count < w->size) {
        // Space is available in the channel. Enqueue to send.
        w->count++;
        spinlock_unlock(&w->lock);
        return true;
    }

    if (!block) {
        spinlock_unlock(&w->lock);
        return false;
    }

    // Block on the channel. Some waiter will complete our operation for us.
    thread_t* thread = get_current_thread();
    wt = acquire_waiting_thread();
    wt->thread = thread;
    wait_queue_enqueue(&w->send_queue, wt);

    // park and release the lock for the waitable
    thread->wait_lock = &w->lock;
    scheduler_park();

    // someone woke us up

    bool closed = !wt->success;
    release_waiting_thread(wt);

    if (closed) {
        return false;
    }

    return true;
}

waitable_result_t waitable_wait(waitable_t* w, bool block) {
    // Fast path: check for failed non-blocking operation without acquiring the lock
    if (!block && waitable_empty(w)) {
        if (w->closed == 0) {
            return WAITABLE_EMPTY;
        }

        if (waitable_empty(w)) {
            return WAITABLE_CLOSED;
        }
    }

    spinlock_lock(&w->lock);

    if (w->closed) {
        if (w->count == 0) {
            spinlock_unlock(&w->lock);
            return WAITABLE_CLOSED;
        }
    } else {
        waiting_thread_t* wt = wait_queue_dequeue(&w->send_queue);
        if (wt != NULL) {
            spinlock_unlock(&w->lock);
            wt->thread->waker = wt;
            wt->success = true;
            scheduler_ready_thread(wt->thread);
            return WAITABLE_SUCCESS;
        }
    }

    if (w->count > 0) {
        // Receive directly
        w->count--;
        spinlock_unlock(&w->lock);
        return WAITABLE_SUCCESS;
    }

    if (!block) {
        spinlock_unlock(&w->lock);
        return WAITABLE_EMPTY;
    }

    // no sender available: block on this waitable
    thread_t* thread = get_current_thread();
    waiting_thread_t* wt = acquire_waiting_thread();
    wt->thread = thread;
    wait_queue_enqueue(&w->wait_queue, wt);

    thread->wait_lock = &w->lock;
    scheduler_park();

    // someone woke us up
    bool success = wt->success;
    release_waiting_thread(wt);
    return success ? WAITABLE_SUCCESS : WAITABLE_CLOSED;
}

void waitable_close(waitable_t* w) {
    spinlock_lock(&w->lock);

    if (w->closed) {
        spinlock_unlock(&w->lock);
        ASSERT(!"Close of closed channel");
        return;
    }

    w->closed = 1;

    thread_t* threads = NULL;

    // release all waiters
    while (true) {
        waiting_thread_t* wt = wait_queue_dequeue(&w->wait_queue);
        if (wt == NULL) {
            break;
        }

        wt->thread->waker = wt;
        wt->success = false;

        // queue for ready
        wt->thread->sched_link = threads;
        threads = wt->thread;
    }

    // release all senders
    while (true) {
        waiting_thread_t* wt = wait_queue_dequeue(&w->send_queue);
        if (wt == NULL) {
            break;
        }

        wt->thread->waker = wt;
        wt->success = false;

        // queue for ready
        wt->thread->sched_link = threads;
        threads = wt->thread;
    }

    spinlock_unlock(&w->lock);

    // Ready all threads now that we've dropped the waitable lock
    while (threads != NULL) {
        thread_t* thread = threads;
        threads = thread->sched_link;
        scheduler_ready_thread(thread);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int waitable_select(waitable_t** waitables, int send_count, int wait_count, bool block) {
    return -1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void send_timer(waitable_t* waitable, uintptr_t now) {
    // non-blocking send
    waitable_send(waitable, false);

    // close the waitable since we are not going to use it anymore
    waitable_close(waitable);

    // release it, we no longer own it
    release_waitable(waitable);
}

waitable_t* after(int64_t microseconds) {
    // create the waitable
    waitable_t* waitable = create_waitable(1);
    if (waitable == NULL) return NULL;

    // create the timer
    timer_t* timer = create_timer();
    if (timer == NULL) {
        SAFE_RELEASE_WAITABLE(waitable);
        return NULL;
    }

    // setup the timer
    timer->when = (int64_t)microtime() + microseconds;
    timer->func = (timer_func_t)send_timer;
    timer->arg = waitable;

    // start it
    timer_start(timer);

    // we don't care about our reference, now it only lives
    // on the timer heap
    release_timer(timer);

    // we return the user its own reference he should release on its own
    // we keep one reference for the send_timer function
    return put_waitable(waitable);
}
