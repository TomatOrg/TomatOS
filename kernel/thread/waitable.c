#include "waitable.h"
#include "scheduler.h"
#include "timer.h"
#include "time/tsc.h"
#include "util/fastrand.h"
#include "time/delay.h"

#include <mem/malloc.h>

waitable_t* create_waitable(size_t size) {
    waitable_t* waitable = malloc(sizeof(waitable_t));
    if (waitable == NULL) {
        return NULL;
    }
    waitable->size = size;
    waitable->ref_count = 1;
    waitable->lock = INIT_SPINLOCK();
    return waitable;
}

waitable_t* put_waitable(waitable_t* waitable) {
    atomic_fetch_add(&waitable->ref_count, 1);
    return waitable;
}

void release_waitable(waitable_t* waitable) {
    if (atomic_fetch_sub(&waitable->ref_count, 1) == 1) {
        if (!waitable->closed) {
            waitable_close(waitable);
        }
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

static void wait_queue_dequeue_wt(wait_queue_t* q, waiting_thread_t* wt) {
    waiting_thread_t* x = wt->prev;
    waiting_thread_t* y = wt->next;
    if (x != NULL) {
        if (y != NULL) {
            // middle of queue
            x->next = y;
            y->prev = x;
            wt->next = NULL;
            wt->prev = NULL;
            return;
        }
        // end of queue
        x->next = NULL;
        q->last = x;
        wt->prev = NULL;
        return;
    }

    if (y != NULL) {
        // start of queue
        y->prev = NULL;
        q->first = y;
        wt->next = NULL;
        return;
    }

    // x == y == NULL. Either wt is the only element in the queue,
    // or it ha already been removed. Use q.first to disambiguate.
    if (q->first == wt) {
        q->first = NULL;
        q->last = NULL;
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
        WARN("waitable: send on closed waitable");
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
        // Space is available in the waitable. Enqueue to send.
        w->count++;
        spinlock_unlock(&w->lock);
        return true;
    }

    if (!block) {
        spinlock_unlock(&w->lock);
        return false;
    }

    // Block on the waitable. Some waiter will complete our operation for us.
    thread_t* thread = get_current_thread();
    wt = acquire_waiting_thread();
    wt->thread = thread;
    wait_queue_enqueue(&w->send_queue, wt);

    // park and release the lock for the waitable
    scheduler_park((void*)spinlock_unlock, &w->lock);

    // someone woke us up

    bool closed = !wt->success;
    release_waiting_thread(wt);

    if (closed) {
        WARN("waitable: send wakeup on closed waitable");
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

    scheduler_park((void*)spinlock_unlock, &w->lock);

    // someone woke us up
    bool success = wt->success;
    release_waiting_thread(wt);
    return success ? WAITABLE_SUCCESS : WAITABLE_CLOSED;
}

void waitable_close(waitable_t* w) {
    spinlock_lock(&w->lock);

    if (w->closed) {
        spinlock_unlock(&w->lock);
        WARN("waitable: close an already closed waitable");
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

static void waitable_select_lock(waitable_t** waitables, const uint16_t* lockorder, int waitables_count) {
    waitable_t* last_w = NULL;
    for (int i = 0; i < waitables_count; i++) {
        waitable_t* w = waitables[lockorder[i]];
        if (last_w != w) {
            last_w = w;
            spinlock_lock(&w->lock);
        }
    }
}

static void waitable_select_unlock(waitable_t** waitables, const uint16_t* lockorder, int waitables_count) {
    for (int i = waitables_count - 1; i >= 0; i--) {
        waitable_t* w = waitables[lockorder[i]];
        if (i > 0 && w == waitables[lockorder[i - 1]]) {
            // will unlock it on the next iteration
            continue;
        }
        spinlock_unlock(&w->lock);
    }
}

static void waitable_select_park(thread_t* thread) {
    waitable_t* last_w = NULL;
    for (waiting_thread_t* wt = thread->waiting; wt != NULL; wt = wt->wait_link) {
        if (wt->waitable != last_w && last_w != NULL) {
            spinlock_unlock(&last_w->lock);
        }
        last_w = wt->waitable;
    }

    if (last_w != NULL) {
        spinlock_unlock(&last_w->lock);
    }
}

selected_waitable_t waitable_select(waitable_t** waitables, int send_count, int wait_count, bool block) {
    int waitable_count = send_count + wait_count;
    ASSERT(waitable_count < UINT16_MAX);

    uint16_t pollorder[waitable_count];
    uint16_t lockorder[waitable_count];

    // generate permuted order
    for (int i = 0; i < waitable_count; i++) {
        uint32_t j = fastrandn(i + 1);
        pollorder[i] = pollorder[j];
        pollorder[j] = i;
    }

    // sort the cases by waitable address to get the locking order.
    // simple heap sort, to guarantee n log n time and constant stack footprint.
    for (int i = 0; i < waitable_count; i++) {
        int j = i;

        waitable_t* w = waitables[pollorder[i]];
        while (j > 0 && waitables[lockorder[(j - 1) / 2]] < w) {
            int k = (j - 1) / 2;
            lockorder[j] = lockorder[k];
            j = k;
        }
        lockorder[j] = pollorder[i];
    }

    for (int i = waitable_count - 1; i >= 0; i--) {
        int o = lockorder[i];
        waitable_t* w = waitables[o];
        lockorder[i] = lockorder[0];
        int j = 0;
        while (true) {
            int k = j * 2 + 1;
            if (k >= i) {
                break;
            }
            if (k + 1 < i && waitables[lockorder[k]] < waitables[lockorder[k + 1]]) {
                k++;
            }
            if (w < waitables[lockorder[k]]) {
                lockorder[j] = lockorder[k];
                j = k;
                continue;
            }
            break;
        }
        lockorder[j] = o;
    }

    // lock all the waitables involved in the select
    waitable_select_lock(waitables, lockorder, waitable_count);

    //
    // pass 1 - look for something already waiting
    //
    for (int o = 0; o < waitable_count; o++) {
        int i = pollorder[o];
        waitable_t* w = waitables[i];

        if (i >= send_count) {
            waiting_thread_t* wt = wait_queue_dequeue(&w->send_queue);
            if (wt != NULL) {
                // can receive from sleeping sender
                waitable_select_unlock(waitables, lockorder, waitable_count);
                wt->thread->waker = wt;
                wt->success = true;
                scheduler_ready_thread(wt->thread);
                return (selected_waitable_t) { .index = i, .success = true };
            }

            if (w->count > 0) {
                // can receieve from waitable
                w->count--;
                waitable_select_unlock(waitables, lockorder, waitable_count);
                return (selected_waitable_t) { .index = i, .success = true };
            }

            if (w->closed) {
                // read at end of closed waitable
                waitable_select_unlock(waitables, lockorder, waitable_count);
                return (selected_waitable_t) { .index = i, .success = false };
            }

        } else {
            if (w->closed != 0) {
                waitable_select_unlock(waitables, lockorder, waitable_count);
                WARN("waitable: select send on closed waitable");
                return (selected_waitable_t) { .index = i, .success = false };
            }

            waiting_thread_t* wt = wait_queue_dequeue(&w->wait_queue);
            if (wt != NULL) {
                waitable_select_unlock(waitables, lockorder, waitable_count);
                wt->thread->waker = wt;
                wt->success = true;
                scheduler_ready_thread(wt->thread);
                return (selected_waitable_t) { .index = i, .success = true };
            }

            if (w->count < w->size) {
                // can send to waitable
                w->count++;
                waitable_select_unlock(waitables, lockorder, waitable_count);
                return (selected_waitable_t) { .index = i, .success = true };
            }
        }
    }

    if (!block) {
        waitable_select_unlock(waitables, lockorder, waitable_count);
        return (selected_waitable_t) { .index = -1, .success = false };
    }

    //
    // pass 2 - enqueue on all chans
    //

    thread_t* thread = get_current_thread();
    ASSERT(thread->waiting == NULL);

    waiting_thread_t** nextp = &thread->waiting;
    for (int o = 0; o < waitable_count; o++) {
        int i = lockorder[o];
        waitable_t* w = waitables[i];

        waiting_thread_t* wt = acquire_waiting_thread();
        wt->thread = thread;
        wt->is_select = true;
        wt->waitable = w;

        // Construct waiting list in lock order.
        *nextp = wt;
        nextp = &wt->wait_link;

        if (i < send_count) {
            wait_queue_enqueue(&w->send_queue, wt);
        } else {
            wait_queue_enqueue(&w->wait_queue, wt);
        }
    }

    // wait for someone to wake us up
    thread->waker = NULL;
    scheduler_park((void*)waitable_select_park, thread);

    waitable_select_lock(waitables, lockorder, waitable_count);

    thread->select_done = 0;
    waiting_thread_t* wt = thread->waker;
    thread->waker = NULL;

    //
    // pass 3 - dequeue from unsuccessful waitables
    //

    waiting_thread_t* wtl = thread->waiting;
    thread->waiting = NULL;

    int waitable_i = -1;
    waitable_t* waitable = NULL;
    bool success = false;
    for (int o = 0; o < waitable_count; o++) {
        int i = lockorder[o];
        waitable_t* w = waitables[i];
        if (wt == wtl) {
            // wt has already been dequeued by the thread that woke us up.
            waitable_i = i;
            waitable = w;
            success = wt->success;
        } else {
            if (i < send_count) {
                wait_queue_dequeue_wt(&w->send_queue, wtl);
            } else {
                wait_queue_dequeue_wt(&w->wait_queue, wtl);
            }
        }

        waiting_thread_t* wtn = wtl->wait_link;
        wtl->wait_link = NULL;
        release_waiting_thread(wtl);
        wtl = wtn;
    }

    ASSERT(waitable != NULL);

    selected_waitable_t selected = {
        .index = waitable_i,
        .success = true
    };

    if (waitable_i < send_count) {
        if (!success) {
            WARN("waitable: select send wakeup on closed waitable");
            selected.success = false;
        }
    } else {
        selected.success = success;
    }

    waitable_select_unlock(waitables, lockorder, waitable_count);

    return selected;
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Self test
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static bool m_self_test_recv = false;

static void self_test_recv_func(waitable_t* w) {
    ASSERT(waitable_wait(w, true) == WAITABLE_SUCCESS);
    m_self_test_recv = true;
    release_waitable(w);
}

static _Atomic(uint32_t) m_self_test_sent = 0;

static void self_test_send_func(waitable_t* w) {
    ASSERT(waitable_send(w, true));
    atomic_store(&m_self_test_sent, 1);
    release_waitable(w);
}

static void self_test_send(waitable_t* w, int count) {
    for (int i = 0; i < count; i++) {
        ASSERT(waitable_send(w, true));
    }
    release_waitable(w);
}

void waitable_self_test() {
    int N = 200;

    TRACE("\tWaitable self-test");

    for (int waitable_cap = 0; waitable_cap < N; waitable_cap++) {
        // Ensure that receive from empty waitable blocks.
        {
            waitable_t* w = create_waitable(waitable_cap);

            m_self_test_recv = false;

            thread_t* t = create_thread((void *) self_test_recv_func, put_waitable(w), "test");
            put_thread(t);
            scheduler_ready_thread(t);

            // I know this is stupid but what can we do
            microdelay(1000);

            ASSERT(!m_self_test_recv && "receive from empty waitable");

            // Ensure that non-blocking receive does not block.
            waitable_t* waitables[] = { w };
            selected_waitable_t selected = waitable_select(waitables, 0, 1, false);
            ASSERT(selected.index == -1 && "receive from empty waitable");
            waitable_send(w, true);

            release_waitable(w);

            while (get_thread_status(t) != THREAD_STATUS_DEAD);
            release_thread(t);
        }

        // Ensure that send to full waitable blocks.
        {
            waitable_t* w = create_waitable(waitable_cap);
            for (int i = 0; i < waitable_cap; i++) {
                waitable_send(w, true);
            }

            m_self_test_sent = 0;

            thread_t* t = create_thread((void*)self_test_send_func, put_waitable(w), "test");
            put_thread(t);
            scheduler_ready_thread(t);

            microdelay(1000);
            ASSERT (atomic_load(&m_self_test_sent) == 0 && "send to full waitable");

            waitable_t* waitables[] = { w };
            selected_waitable_t selected = waitable_select(waitables, 1, 0, false);
            ASSERT(selected.index == -1 && "send to full waitable");

            ASSERT(waitable_wait(w, true) == WAITABLE_SUCCESS);

            release_waitable(w);

            while (get_thread_status(t) != THREAD_STATUS_DEAD);
            release_thread(t);
        }

        // Ensure that we receive 0 from closed chan.
        {
            // TODO: this
        }

        // Ensure that close unblocks receive.
        {
            // TODO: this
        }

        // Send 100
        {
            waitable_t* w = create_waitable(waitable_cap);

            thread_t* t = create_thread((void*) self_test_send, put_waitable(w), "test");
            t->save_state.rsi = 100;
            scheduler_ready_thread(t);

            for (int i = 0; i < 100; i++) {
                ASSERT(waitable_wait(w, true) == WAITABLE_SUCCESS);
            }

            release_waitable(w);
        }


        // Send 1000 in 4 threads,
        {
            waitable_t* w = create_waitable(waitable_cap);

            for (int i = 0; i < 4; i++) {
                thread_t* t = create_thread((void*) self_test_send, put_waitable(w), "test");
                t->save_state.rsi = 1000;
                scheduler_ready_thread(t);
            }

            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 1000; j++) {
                    ASSERT(waitable_wait(w, true) == WAITABLE_SUCCESS);
                }
            }

            release_waitable(w);
        }
    }
}
