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

static int m_color_black = 0;
static int m_color_white = 1;

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

/**
 * Singly linked list of all the allocated objects in the system
 */
static _Atomic(System_Object) m_all_objects = NULL;

static mutex_t m_global_roots_mutex;
static System_Object** m_global_roots = NULL;

void gc_add_root(void* object) {
    mutex_lock(&m_global_roots_mutex);
    arrpush(m_global_roots, object);
    mutex_unlock(&m_global_roots_mutex);
}

static void add_object(System_Object o) {
    // add to the all objects list atomically
    o->next = atomic_load_explicit(&m_all_objects, memory_order_relaxed);
    while (!atomic_compare_exchange_weak_explicit(&m_all_objects, &o->next, o, memory_order_relaxed, memory_order_relaxed));
}

void* gc_new(System_Type type, size_t size) {
    scheduler_preempt_disable();

    System_Object o = heap_alloc(size);
    memset(o, 0, size);
    o->next = NULL;
    o->color = GCL->alloc_color;
    o->type = type;
    add_object(o);

    scheduler_preempt_enable();

    return o;
}

void gc_update(void* o, size_t offset, void* new) {
    scheduler_preempt_disable();

    System_Object object = o;

    if (GCL->trace_on && object->color == m_color_white) {
        if (object->log_pointer) { // object not dirty
            int temp_pos = arrlen(GCL->buffer);

            // Get a snapshot of the object
            if (object->type->IsArray) {
                // array object, check if we need to trace items
                if (!object->type->ElementType->IsValueType) {
                    System_Array array = o;
                    arrsetcap(GCL->buffer, arrlen(GCL->buffer) + array->Length + 1);
                    for (int i = 0; i < array->Length; i++) {
                        size_t offset = sizeof(struct System_Array) +  + i * sizeof(void*);
                        GCL->buffer[++temp_pos] = read_field(o, offset);
                    }

                    GCL->buffer[++temp_pos] = (System_Object)array->type;
                }
            } else {
                // normal object, trace its fields
                int* managed_pointer_offsets = object->type->ManagedPointersOffsets;
                arrsetcap(GCL->buffer, arrlen(GCL->buffer) + arrlen(managed_pointer_offsets));
                for (int i = 0; i < arrlen(managed_pointer_offsets); i++) {
                    GCL->buffer[++temp_pos] = read_field(o, managed_pointer_offsets[i]);
                }
            }

            // is it still not dirty?
            if (object->log_pointer == NULL) {
                // committing values in buffer
                arrsetlen(GCL->buffer, temp_pos);

                // set dirty
                object->log_pointer = &GCL->buffer[temp_pos];
            }
        }
    }

    write_field(o, offset, new);

    if (GCL->snoop && new != NULL) {
        stbds_hmput(GCL->snooped, new, new);
    }

    scheduler_preempt_enable();
}

static void initiate_collection_cycle() {
    // first handshake
    lock_all_threads();
    m_default_gc_thread_data.snoop = true;
    for (int i = 0; i < arrlen(g_all_threads); i++) {
        thread_t* thread = g_all_threads[i];
        if (thread == get_current_thread()) continue;

        suspend_state_t state = scheduler_suspend_thread(thread);
        thread->tcb->gc_data.snoop = true;
        scheduler_resume_thread(state);
    }
    unlock_all_threads();

    // give a small room for new threads to be created...

    // second handshake
    lock_all_threads();
    m_default_gc_thread_data.trace_on = true;
    for (int i = 0; i < arrlen(g_all_threads); i++) {
        thread_t* thread = g_all_threads[i];
        if (thread == get_current_thread()) continue;

        suspend_state_t state = scheduler_suspend_thread(thread);
        thread->tcb->gc_data.trace_on = true;
        scheduler_resume_thread(state);
    }
    unlock_all_threads();
}

static object_set_t m_roots = NULL;

static void get_roots() {
    m_color_black = 1 - m_color_black;
    m_color_white = 1 - m_color_white;

    // third handshake
    lock_all_threads();
    m_default_gc_thread_data.alloc_color = m_color_black;
    m_default_gc_thread_data.snoop = false;
    for (int i = 0; i < arrlen(g_all_threads); i++) {
        thread_t* thread = g_all_threads[i];
        if (thread == get_current_thread()) continue;

        suspend_state_t state = scheduler_suspend_thread(thread);
        gc_thread_data_t* gcl = &thread->tcb->tcb->gc_data;

        gcl->alloc_color = m_color_black;
        gcl->snoop = false;

        // TODO: copy thread local state...

        scheduler_resume_thread(state);
    }
    unlock_all_threads();

    // give a small room for new threads to be created...

    lock_all_threads();
    for (int i = 0; i < arrlen(g_all_threads); i++) {
        thread_t* thread = g_all_threads[i];
        if (thread == get_current_thread()) continue;

        // copy and clear snooped objects set
        for (int j = 0; j < hmlen(thread->tcb->gc_data.snooped); j++) {
            System_Object o = thread->tcb->gc_data.snooped[j].key;
            hmput(m_roots, o, o);
        }
        hmfree(thread->tcb->gc_data.snooped);
    }
    unlock_all_threads();

    // add all the globals to the current roots
    mutex_lock(&m_global_roots_mutex);
    for (int i = 0; i < arrlen(m_global_roots); i++) {
        System_Object o = *m_global_roots[i];
        hmput(m_roots, o, o);
    }
    mutex_unlock(&m_global_roots_mutex);
}

static System_Object* m_mark_stack = NULL;

#if 0
    #define TRACE_PRINT(...) TRACE(__VA_ARGS__)
#else
    #define TRACE_PRINT(...)
#endif

#if 0
    #define SWEEP_PRINT(...) TRACE(__VA_ARGS__)
#else
    #define SWEEP_PRINT(...)
#endif

static void trace(System_Object o) {
    TRACE_PRINT("tracing %p (%U.%U)", o, o->type->Namespace, o->type->Name);

    if (o->color == m_color_white) {
        if (o->log_pointer == NULL) {
            // if not dirty

            // getting a replica
            size_t count = 0;
            System_Object* temp = NULL;
            if (o->type->IsArray && !o->type->ElementType->IsValueType) {
                // array object, check if we need to trace items
                System_Array array = (System_Array)o;
                count = array->Length + 1;
                temp = __builtin_alloca(count * sizeof(System_Object));
                for (int i = 0; i < array->Length; i++) {
                    size_t offset = sizeof(struct System_Array) + i * sizeof(void*);
                    temp[i] = read_field(o, offset);
                    TRACE_PRINT("\tadding %p (array item %d)", temp[i], i);
                }

                // don't forget about the Type object
                temp[array->Length] = (System_Object)array->type;
                TRACE_PRINT("\tadding %p (array type)", temp[array->Length]);
            } else {
                // normal object, trace its fields
                int* managed_pointer_offsets = o->type->ManagedPointersOffsets;
                count = arrlen(managed_pointer_offsets);
                temp = __builtin_alloca(count * sizeof(System_Object));
                for (int i = 0; i < arrlen(managed_pointer_offsets); i++) {
                    temp[i] = read_field(o, managed_pointer_offsets[i]);
                    TRACE_PRINT("\tadding %p (offset 0x%02x)", temp[i], managed_pointer_offsets[i]);
                }
            }

            if (o->log_pointer == NULL) {
                for (int i = 0; i < count; i++) {
                    if (temp[i] == NULL) continue;
                    arrpush(m_mark_stack, temp[i]);
                }
            }
        } else {
            // object is dirty

            size_t count = 0;
            if (o->type->IsArray) {
                // array object, check if we need to trace items
                if (!o->type->ElementType->IsValueType) {
                    System_Array array = (System_Array)o;
                    count = array->Length + 1;
                }
            } else {
                // normal object, trace its fields
                int* managed_pointer_offsets = o->type->ManagedPointersOffsets;
                count = arrlen(managed_pointer_offsets);
            }

            for (int i = 0; i < count; i++) {
                int ni = count - i - 1;
                if (o->log_pointer[ni] == NULL) continue;
                TRACE_PRINT("\tadding %p (dirty)", o->log_pointer[ni]);
                arrpush(m_mark_stack, o->log_pointer[ni]);
            }
        }

        o->color = m_color_black;
    }
}

static void trace_heap() {
    for (int i = 0; i < hmlen(m_roots); i++) {
        System_Object o = m_roots[i].key;
        if (o == NULL) continue;
        arrpush(m_mark_stack, o);
    }

    while (arrlen(m_mark_stack) != 0) {
        System_Object o = arrpop(m_mark_stack);
        trace(o);
    }
}

static void revive_object(System_Object o) {
    // only green objects (marked for finalization) or white objects (unreachable)
    // should be revived at this stage
    if (o == NULL || (o->color != m_color_white && o->color != GC_COLOR_GREEN)) {
        return;
    }

    o->color = m_color_black;

    // we can safely iterate all the fields of this object since it is already dead
    if (o->type->IsArray && !o->type->ElementType->IsValueType) {
        // array object, check if we need to revive items
        System_Array array = (System_Array)o;
        for (int i = 0; i < array->Length; i++) {
            size_t offset = sizeof(struct System_Array) + i * sizeof(void*);
            revive_object(read_field(o, offset));
        }

        // don't forget about the Type object
        revive_object((System_Object)array->type);
    } else {
        // normal object, revive its fields
        int* managed_pointer_offsets = o->type->ManagedPointersOffsets;
        for (int i = 0; i < arrlen(managed_pointer_offsets); i++) {
            revive_object(read_field(o, managed_pointer_offsets[i]));
        }
    }
}

static void sweep() {
    // fourth handshake
    lock_all_threads();
    m_default_gc_thread_data.trace_on = false;
    for (int i = 0; i < arrlen(g_all_threads); i++) {
        thread_t* thread = g_all_threads[i];
        if (thread == get_current_thread()) continue;
        suspend_state_t state = scheduler_suspend_thread(thread);
        thread->tcb->gc_data.trace_on = false;
        scheduler_resume_thread(state);
    }
    unlock_all_threads();

    // linked list of all swept objects
    System_Object swept = NULL;

    // special handling for the iter object
    System_Object last = NULL;
    System_Object iter = atomic_load_explicit(&m_all_objects, memory_order_relaxed);
    while (iter != NULL) {
        // save the next for when we need it
        System_Object next = iter->next;

        if (iter->color == m_color_white) {
            // remove from the queue
            if (last == NULL) {
                // removing the first object is a bit special
                // TODO: maybe we can do this just with exchange :think:
                System_Object first_now = iter;
                if (!atomic_compare_exchange_weak_explicit(&m_all_objects, &first_now, iter->next, memory_order_relaxed, memory_order_relaxed)) {
                    // the compare exchange failed, this means that the iter is no longer
                    // the first item and the object that we have in our hand is the new first
                    // object, starting from it go to the next until we find the current item,
                    // setting the last along the way
                    do {
                        last = first_now;
                        first_now = first_now->next;
                    } while (first_now != iter);

                    // we found it, remove
                    last->next = iter->next;
                } else {
                    // the last is kept as NULL because it is now
                }
            } else {
                // easy case, we know the last, just remove it
                last->next = iter->next;
            }

            // queue this for finalization
            TRACE_PRINT("Sweeping object at %p of type %U.%U", iter, iter->type->Namespace, iter->type->Name);
            iter->next = swept;
            swept = iter;
        } else {
            // the last object is this one since it is still alive
            last = iter;
        }

        // set the iter item to be the next item
        iter = next;
    }

    // revive objects that should be finalized
    iter = swept;
    while (iter != NULL) {
        if (!iter->suppress_finalizer && iter->type->Finalize != NULL && iter->color == m_color_white) {
            // this object has a finalizer and was not revived yet
            revive_object(iter);

            // this is now green
            iter->color = GC_COLOR_GREEN;
        }
        iter = iter->next;
    }

    // now either free or queue for finalization
    iter = swept;
    while (iter != NULL) {
        System_Object current = iter;
        iter = iter->next;

        if (current->color == m_color_white) {
            SWEEP_PRINT("Freeing object at %p of type %U.%U", current, current->type->Namespace, current->type->Name);
            current->color = GC_COLOR_BLUE;
            heap_free(current);
        } else if (current->color == m_color_black) {
            SWEEP_PRINT("Reviving object at %p of type %U.%U", current, current->type->Namespace, current->type->Name);
            add_object(current);
        } else if (current->color == GC_COLOR_GREEN) {
            SWEEP_PRINT("Finalizing object at %p of type %U.%U", current, current->type->Namespace, current->type->Name);
            current->color = m_color_black;
            current->suppress_finalizer = true;
            // TODO: figure on which thread to run this
            add_object(current);
        }
    }

}

static void prepare_next_collection() {
    hmfree(m_roots);

    lock_all_threads();
    for (int i = 0; i < arrlen(g_all_threads); i++) {
        thread_t* thread = g_all_threads[i];
        if (thread == get_current_thread()) continue;

        // Clear all log pointers
        System_Object* buffer = thread->tcb->gc_data.buffer;
        for (int j = 0; j < arrlen(buffer); j++) {
            buffer[j]->log_pointer = NULL;
        }

        // clear objects buffer
        arrfree(thread->tcb->gc_data.buffer);
    }
    unlock_all_threads();
}

static void gc_collection_cycle() {
    initiate_collection_cycle();
    get_roots();
    trace_heap();
    sweep();
    prepare_next_collection();

    // TODO: heap_flush every so often
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Conductor, allows mutators to trigger the gc
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GC Thread, actually does the garbage collection
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

noreturn static void gc_thread(void* ctx) {
    TRACE("gc: GC thread started");

    while (true) {
        mutex_lock(&m_gc_mutex);
        gc_conductor_next();
        mutex_unlock(&m_gc_mutex);
        TRACE("gc: Starting collection");

        uint64_t start = microtime();
        gc_collection_cycle();
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
