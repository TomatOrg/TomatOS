#include "gc.h"

#include "gc_thread_data.h"
#include "heap.h"

#include <sync/conditional.h>
#include <sync/wait_group.h>

#include <proc/scheduler.h>
#include <proc/thread.h>

#include <util/stb_ds.h>
#include "time/timer.h"

#include <stdnoreturn.h>
#include <stdatomic.h>

/**
 * Get the gc local data as fs relative pointer, this should allow the compiler
 * to nicely optimize this access, while still allowing us to access it from the
 * tcb which is stored in the thread struct
 */
#define GTD ((gc_thread_data_t __seg_fs*)offsetof(thread_control_block_t, gc_data))

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
static _Atomic(bool) m_gc_tracing = false;

/**
 * The gc/collector thread
 */
static thread_t* m_collector_thread = NULL;

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

static spinlock_t m_global_roots_lock;
static System_Object** m_global_roots = NULL;

void gc_add_root(void* object) {
    spinlock_lock(&m_global_roots_lock);
    arrpush(m_global_roots, object);
    spinlock_unlock(&m_global_roots_lock);
}

void* gc_new(System_Type type, size_t size) {
    scheduler_preempt_disable();

    // allocate the object
    System_Object o = heap_alloc(size, m_allocation_color);

    // set the object type
    if (type != NULL) {
        ASSERT(type->VTable != NULL);
        o->vtable = type->VTable;
    }

    // if there is no finalize then always suppress the finalizer
    if (type != NULL) {
        o->suppress_finalizer = type->Finalize == NULL;
    }

    scheduler_preempt_enable();

    return o;
}

/**
 * Used to tell the collector that there are more gray objects
 */
static _Atomic(bool) m_gc_has_gray_objects = false;

static void gc_mark_gray(System_Object object) {
    if (
        object != NULL &&
        (
            object->color == m_clear_color ||
            (object->color == m_allocation_color && GTD->status != THREAD_STATUS_ASYNC)
        )
    ) {
        object->color = COLOR_GRAY;
        m_gc_has_gray_objects = true;
    } else {
        if (object != NULL && object->color == m_allocation_color) {
            ASSERT(!"should have been marked!");
        }
    }
}

void gc_update(void* o, size_t offset, void* new) {
    scheduler_preempt_disable();

    if (GTD->status != THREAD_STATUS_ASYNC) {
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

//----------------------------------------------------------------------------------------------------------------------
// Handshaking with all the threads, async to the main collector
//----------------------------------------------------------------------------------------------------------------------

static const char* m_status_str[] = {
    [THREAD_STATUS_ASYNC] = "ASYNC",
    [THREAD_STATUS_SYNC1] = "SYNC1",
    [THREAD_STATUS_SYNC2] = "SYNC2",
};

static void gc_mark_ptr(uintptr_t ptr) {
    System_Object object = heap_find(ptr);
    if (object != NULL) {
        gc_mark_gray(object);
    }
}

static wait_group_t m_gc_handshake_wg = { 0 };

static void gc_handshake_thread(void* arg) {
    gc_thread_status_t status = (gc_thread_status_t)(uintptr_t)arg;

    // set our own status so gc_mark_gray will know
    // our status
    GTD->status = status;

    // iterate over all mutators, suspend them, and
    // set the status as needed, if more work is needed
    // do it now
    lock_all_threads();

    // set the default status for the next threads that will be created
    m_default_gc_thread_data.status = status;

    for (int i = 0; i < arrlen(g_all_threads); i++) {
        // get the thread and suspend it if it is not us
        // TODO: optimize by only doing the threads that don't run right now or something
        // TODO: skip any gc thread
        thread_t* thread = g_all_threads[i];

        // don't suspend either our thread or the collector thread
        if (thread == get_current_thread() || thread == m_collector_thread) continue;

        // suspend and get the thread state
        suspend_state_t state = scheduler_suspend_thread(thread);
        gc_thread_data_t* gcl = &thread->tcb->tcb->gc_data;

        // sync2 == finding roots
        if (status == THREAD_STATUS_SYNC2) {

            // mark the stack
            for (uintptr_t ptr = ALIGN_UP(thread->save_state.rsp - 128, 8); ptr <= (uintptr_t)thread->stack_top - 8; ptr += 8) {
                gc_mark_ptr(*((uintptr_t*)(ptr)));
            }

            // mark the registers
            gc_mark_ptr(thread->save_state.r15);
            gc_mark_ptr(thread->save_state.r14);
            gc_mark_ptr(thread->save_state.r13);
            gc_mark_ptr(thread->save_state.r12);
            gc_mark_ptr(thread->save_state.r11);
            gc_mark_ptr(thread->save_state.r10);
            gc_mark_ptr(thread->save_state.r9);
            gc_mark_ptr(thread->save_state.r8);
            gc_mark_ptr(thread->save_state.rbp);
            gc_mark_ptr(thread->save_state.rdi);
            gc_mark_ptr(thread->save_state.rsi);
            gc_mark_ptr(thread->save_state.rdx);
            gc_mark_ptr(thread->save_state.rcx);
            gc_mark_ptr(thread->save_state.rbx);
            gc_mark_ptr(thread->save_state.rax);
        }

        // set the status
        gcl->status = status;

        // resume the threads operation
        scheduler_resume_thread(state);
    }
    unlock_all_threads();

    wait_group_done(&m_gc_handshake_wg);
}

static void gc_post_handshake(gc_thread_status_t status) {
    // add the work
    wait_group_add(&m_gc_handshake_wg, 1);

    // set the status of our thread so everything will sync nicely
    GTD->status = status;

    // create the handshake thread and ready it
    thread_t* thread = create_thread(gc_handshake_thread, (void*)status, "gc/handshake[%s]", m_status_str[status]);
    if (thread == NULL) {
        // TODO: panic
        ASSERT(!"failed to create gc_post_handshake thread");
    }
    scheduler_ready_thread(thread);
}

static void gc_wait_handshake() {
    wait_group_wait(&m_gc_handshake_wg);
}

static void gc_handshake(gc_thread_status_t status) {
    gc_post_handshake(status);
    gc_wait_handshake();
}

//----------------------------------------------------------------------------------------------------------------------
// Actual collector logic
//----------------------------------------------------------------------------------------------------------------------

static void gc_set_allocation_color(System_Object object) {
    // set all
    if (object->color == COLOR_BLACK || object->color == COLOR_GRAY) {
        object->color = m_allocation_color;
    }
}

static void gc_init_full_collection() {
    // set allocation color for all objects
    heap_iterate_objects(gc_set_allocation_color);

    // clear all the dirty bits
    heap_iterate_dirty_objects(NULL);
}

static void gc_clear_cards_callback(System_Object object) {
    if (object->color == COLOR_BLACK) {
        object->color = COLOR_GRAY;
        m_gc_has_gray_objects = true;
    }
}

static void gc_clear_cards() {
    heap_iterate_dirty_objects(gc_clear_cards_callback);
}

static void gc_clear(bool full_collection) {
    if (full_collection) {
        gc_init_full_collection();
    }
    gc_handshake(THREAD_STATUS_SYNC1);
}

static void gc_switch_allocation_clear_colors() {
    int temp = m_clear_color;
    m_clear_color = m_allocation_color;
    m_allocation_color = temp;
}

static void gc_mark_global_roots() {
    spinlock_lock(&m_global_roots_lock);
    for (int i = 0; i < arrlen(m_global_roots); i++) {
        System_Object object = *m_global_roots[i];
        if (object->color == m_clear_color || object->color == m_allocation_color) {
            gc_mark_gray(object);
        }
    }
    spinlock_unlock(&m_global_roots_lock);

    // get all the app domains and iterate all their assemblies and stuff
}

static void gc_mark_black(System_Object object) {
    // mark all the children as gray
    if (object->vtable->type->IsArray && !object->vtable->type->ElementType->IsValueType) {
        // array object, mark all the items
        System_Array array = (System_Array)object;
        for (int i = 0; i < array->Length; i++) {
            size_t offset = sizeof(struct System_Array) + i * sizeof(void*);
            System_Object o = read_field(object, offset);
            gc_mark_gray(o);
        }
    } else {
        // for normal objects iterate
        for (int i = 0; i < arrlen(object->vtable->type->ManagedPointersOffsets); i++) {
            gc_mark_gray(read_field(object, object->vtable->type->ManagedPointersOffsets[i]));
        }
    }

    // don't forget about the Type object
    gc_mark_gray((System_Object)object->vtable->type);

    // mark this as black
    object->color = COLOR_BLACK;
}

static void gc_trace_gray(System_Object object) {
    // skip non-gray objects
    if (object->color != COLOR_GRAY) return;

    // mark as black
    gc_mark_black(object);
}

static void gc_complete_trace() {
    while (m_gc_has_gray_objects) {
        m_gc_has_gray_objects = false;
        heap_iterate_objects(gc_trace_gray);
    }
}

static void gc_mark() {
    gc_post_handshake(THREAD_STATUS_SYNC2);
    gc_clear_cards();
    gc_switch_allocation_clear_colors();
    gc_wait_handshake();

    gc_post_handshake(THREAD_STATUS_ASYNC);
    gc_mark_global_roots();
    gc_complete_trace();
    gc_wait_handshake();
}

static void gc_trace() {
    gc_complete_trace();
}

static void gc_free_clear_objects(System_Object object) {
    if (object->color == m_clear_color) {
        if (!object->suppress_finalizer) {
            // the object has a finalizer that we need to run!
            WARN("TODO: run finalizers, leaking memory right now");
        }

        heap_free(object);
    }
}

static void gc_sweep(bool full_collection) {
    // we are going to free all the objects that have a clear color,
    // so they can be used for allocations
    heap_iterate_objects(gc_free_clear_objects);

    if (full_collection) {
        // in a full collection we are also going to reclaim memory back
        // from the collector to the pmm, this is a bit slower so only
        // done on full collection
        heap_reclaim();
    }
}

static void gc_collection_cycle(bool full_collection) {
    gc_clear(full_collection);
    gc_mark();

    m_gc_tracing = true;
    gc_trace();
    gc_sweep(full_collection);
    m_gc_tracing = false;
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
        TRACE("gc: Collection finished after %dms", (microtime() - start) / 1000);
    }
}

err_t init_gc() {
    err_t err = NO_ERROR;

    m_collector_thread = create_thread(gc_thread, NULL, "gc/collector");
    CHECK(m_collector_thread != NULL);
    scheduler_ready_thread(m_collector_thread);

    mutex_lock(&m_gc_mutex);
    gc_conductor_wait();
    mutex_unlock(&m_gc_mutex);

cleanup:
    return err;
}
