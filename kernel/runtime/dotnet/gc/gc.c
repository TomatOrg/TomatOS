#include "gc.h"

#include "gc_thread_data.h"
#include "heap.h"
#include "time/timer.h"

#include <proc/scheduler.h>
#include <proc/thread.h>
#include <sync/conditional.h>
#include <util/stb_ds.h>

#include <stdnoreturn.h>
#include <stdatomic.h>


/**
 * Get the gc local data as fs relative pointer, this should allow the compiler
 * to nicely optimize this access, while still allowing us to access it from the
 * tcb which is stored in the thread struct
 */
#define GCL ((gc_thread_data_t __seg_fs*)offsetof(thread_control_block_t, gc_data))

gc_thread_data_t m_default_gc_thread_data;

/**
 * The color used for allocation, switched with clear
 * color on collection
 */
static int m_allocation_color = COLOR_WHITE;

/**
 * The color used for clearing objects, switched with
 * allocation color on collection
 */
static int m_clear_color = COLOR_YELLOW;

/**
 * Is the garbage collector tracing right now
 */
static bool m_gc_tracing = false;

/**
 * Read object field
 */
static System_Object read_field(void* o, size_t offset) {
    return *(System_Object*)((uintptr_t)o + offset);
}

/**
 * Write to a pointer field
 */
static void write_field(void* o, size_t offset, void* new) {
    *(void**)((uintptr_t)o + offset) = new;
}

static mutex_t m_global_roots_mutex;
static System_Object** m_global_roots = NULL;

void gc_add_root(void* object) {
    mutex_lock(&m_global_roots_mutex);
    arrpush(m_global_roots, object);
    mutex_unlock(&m_global_roots_mutex);
}

void* gc_new(System_Type type, size_t size) {
    scheduler_preempt_disable();

    System_Object o = heap_alloc(size);
    o->color = m_allocation_color;
    o->type = type;

    scheduler_preempt_enable();

    return o;
}

static void gc_mark_gray(System_Object object) {
    if (
        object->color == m_clear_color ||
        object->color == m_allocation_color && GCL->status != THREAD_STATUS_ASYNC
    ) {
        object->color = COLOR_GRAY;
    }
}

void gc_update(void* o, size_t offset, void* new) {
    scheduler_preempt_disable();

    if (GCL->status != THREAD_STATUS_ASYNC) {
        gc_mark_gray(o);
        gc_mark_gray(new);
    } else if (m_gc_tracing) {
        gc_mark_gray(o);
        // Mark card, done implicitly because we
        // are going to change the object
    } else {
        // Mark card, done implicitly because we
        // are going to change the object
    }

    // set it
    write_field(o, offset, new);

    scheduler_preempt_enable();
}

//static void gc_init_full_collection() {
//
//}
//
//static const char* m_status_str[] = {
//    [THREAD_STATUS_ASYNC] = "ASYNC",
//    [THREAD_STATUS_SYNC1] = "SYNC1",
//    [THREAD_STATUS_SYNC2] = "SYNC2",
//};
//
//static mutex_t m_gc_handshake_mutex;
//static conditional_t m_gc_handshake_cond;
//
//static void gc_handshake_thread(void* arg) {
//    gc_thread_status_t status = (gc_thread_status_t)(uintptr_t)arg;
//
//    TRACE("gc: handshake for %s started", m_status_str[status]);
//    uint64_t start = microtime();
//
//    // iterate over all mutators, suspend them, and
//    // set the status as needed, if more work is needed
//    // do it now
//    lock_all_threads();
//    for (int i = 0; i < arrlen(g_all_threads); i++) {
//        // get the thread and suspend it if it is not us
//        // TODO: optimize by only doing the threads that don't run right now or something
//        // TODO: skip any gc thread
//        thread_t* thread = g_all_threads[i];
//        if (thread == get_current_thread()) continue;
//        suspend_state_t state = scheduler_suspend_thread(thread);
//        gc_thread_data_t* gcl = &thread->tcb->tcb->gc_data;
//
//        if (status == THREAD_STATUS_SYNC2) {
//            // scan the stack for roots, include red-zone because idk if the JIT is
//            // going to use it or not
//            for (uintptr_t ptr = (uintptr_t)thread->stack_top; ptr <= thread->save_state.rsp - 128 + 8; ptr -= 8) {
//                System_Object object = heap_find((void*)ptr);
//                if (object != NULL) {
//                    gc_mark_gray(object);
//                }
//            }
//        }
//
//        // set the status
//        gcl->status = status;
//
//        // resume the threads operation
//        scheduler_resume_thread(state);
//    }
//    unlock_all_threads();
//
//    TRACE("gc: Collection finished after %dus", microtime() - start);
//
//    // signal we are finished and exit the thread
//    conditional_signal(&m_gc_handshake_cond);
//}
//
//static void gc_post_handshake(gc_thread_status_t status) {
//    // create the handshake thread and ready it
//    thread_t* thread = create_thread(gc_handshake_thread, (void*)status, "gc/handshake[%s]", m_status_str[status]);
//    if (thread == NULL) {
//        // TODO: panic
//        ASSERT(!"failed to create gc_post_handshake thread");
//    }
//    scheduler_ready_thread(thread);
//}
//
//static void gc_wait_handshake() {
//    mutex_lock(&m_gc_handshake_mutex);
//    conditional_wait(&m_gc_handshake_cond, &m_gc_handshake_mutex);
//    mutex_unlock(&m_gc_handshake_mutex);
//}
//
//static void gc_handshake(gc_thread_status_t status) {
//    gc_post_handshake(status);
//    gc_wait_handshake();
//}
//
//static void gc_clear_cards() {
//    heap_clear_cards();
//}
//
//static void gc_clear(bool full_collection) {
//    if (full_collection) {
//        gc_init_full_collection();
//    }
//    gc_handshake(THREAD_STATUS_SYNC1);
//}
//
//static void gc_switch_allocation_clear_colors() {
//    int temp = m_clear_color;
//    m_clear_color = m_allocation_color;
//    m_allocation_color = temp;
//}
//
//static void gc_mark_global_roots() {
//
//}
//
//static void gc_complete_trace() {
//
//}
//
//static void gc_mark() {
//    gc_post_handshake(THREAD_STATUS_SYNC2);
//    gc_clear_cards();
//    gc_switch_allocation_clear_colors();
//    gc_wait_handshake();
//
//    gc_post_handshake(THREAD_STATUS_ASYNC);
//    gc_mark_global_roots();
//    gc_complete_trace();
//    gc_wait_handshake();
//}
//
//static void gc_trace() {
//    gc_complete_trace();
//}
//
//static void gc_sweep() {
//    // TODO: for object in heap clear
//}

static void gc_collection_cycle(bool full_collection) {
//    gc_clear(full_collection);
//    gc_mark();
//    gc_trace();
//    gc_sweep();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GC Main thread
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------------------------------------------------
// Conductor, allows mutators to trigger the gc
//----------------------------------------------------------------------------------------------------------------------

/**
 * Is the gc currently running
 */
static atomic_bool m_gc_running = true;

/**
 * Mutex for controlling the running state
 */
static mutex_t m_gc_mutex = { 0 };

/**
 * Conditional variable for waking the garbage collector
 */
static conditional_t m_gc_wake = { 0 };

/**
 * Conditional variable for waiting for the gc to be finished on the cycle
 */
static conditional_t m_gc_done = { 0 };

/**
 * Allows the gc to wait until the next request for a collection
 */
static void gc_conductor_next() {
    m_gc_running = false;
    conditional_broadcast(&m_gc_done);
    do {
        conditional_wait(&m_gc_wake, &m_gc_mutex);
    } while (!m_gc_running);
}

/**
 * Wakeup the garbage collector
 */
static void gc_conductor_wake() {
    if (m_gc_running) {
        // gc is already running or someone
        // already requested it to run
        return;
    }

    m_gc_running = true;
    conditional_signal(&m_gc_wake);
}

/**
 * Wait for the garbage collector
 */
static void gc_conductor_wait() {
    do {
        conditional_wait(&m_gc_done, &m_gc_mutex);
    } while (m_gc_running);
}

void gc_wake() {
    gc_conductor_wake();
}

void gc_wait() {
    mutex_lock(&m_gc_mutex);
    gc_conductor_wake();
    gc_conductor_wait();
    mutex_unlock(&m_gc_mutex);
}

noreturn static void gc_thread(void* ctx) {
    TRACE("gc: GC thread started");

    while (true) {
        mutex_lock(&m_gc_mutex);
        gc_conductor_next();
        mutex_unlock(&m_gc_mutex);
        TRACE("gc: Starting collection");

        uint64_t start = microtime();
        gc_collection_cycle(true);
        TRACE("gc: Collection finished after %dus", microtime() - start);
    }
}

err_t init_gc() {
    err_t err = NO_ERROR;

    thread_t* thread = create_thread(gc_thread, NULL, "kernel/gc");
    CHECK(thread != NULL);
    scheduler_ready_thread(thread);

    mutex_lock(&m_gc_mutex);
    gc_conductor_wait();
    mutex_unlock(&m_gc_mutex);

cleanup:
    return err;
}
