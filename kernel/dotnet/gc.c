#include "gc.h"

#include "types.h"
#include "util/stb_ds.h"
#include "sync/conditional.h"

#include <threading/scheduler.h>
#include <threading/thread.h>
#include <stdatomic.h>
#include <stdnoreturn.h>

/**
 * Get the gc local data as fs relative pointer, this should allow the compiler
 * to nicely optimize this access, while still allowing us to access it from the
 * tcb which is stored in the thread struct
 */
#define GCL ((gc_local_data_t __seg_fs*)offsetof(thread_control_block_t, gc_local_data))

static int m_color_black = 0;
static int m_color_white = 1;

static object_t* read_field(object_t* o, size_t offset) {
    return *(object_t**)((uintptr_t)o + offset);
}

static void write_field(object_t* o, size_t offset, object_t* new) {
    *(object_t**)((uintptr_t)o + offset) = new;
}

static spinlock_t m_all_objects_lock = INIT_SPINLOCK();
static list_t m_all_objects = INIT_LIST(m_all_objects);

object_t* gc_new(type_t* type, size_t count) {
    scheduler_preempt_disable();

    object_t* o = malloc(type->managed_size * count);
    o->color = GCL->alloc_color;
    o->type = type;

    TRACE("Allocated %p", o);

    spinlock_lock(&m_all_objects_lock);
    list_add(&m_all_objects, &o->link);
    spinlock_unlock(&m_all_objects_lock);

    scheduler_preempt_enable();
    return o;
}

void gc_update(object_t* o, size_t offset, object_t* new) {
    scheduler_preempt_disable();

    if (GCL->trace_on && o->color == m_color_white) {
        if (o->log_pointer) { // object not dirty
            int temp_pos = arrlen(GCL->buffer);

            size_t* managed_pointer_offsets = o->type->managed_pointer_offsets;
            arrsetcap(GCL->buffer, arrlen(GCL->buffer) + arrlen(managed_pointer_offsets) + 1);
            for (int i = 0; i < arrlen(managed_pointer_offsets); i++) {
                GCL->buffer[++temp_pos] = read_field(o, managed_pointer_offsets[i]);
            }

            // is it still not dirty?
            if (o->log_pointer == NULL) {
                // add pointer to object
//                GCL->buffer[++temp_pos] = (object_t*)((uintptr_t) o | 1);

                // committing values in buffer
                arrsetlen(GCL->buffer, temp_pos);

                // set dirty
                o->log_pointer = &GCL->buffer[temp_pos];
            }
        }
    }

    write_field(o, offset, new);

    if (GCL->snoop && new != NULL) {
        stbds_hmput(GCL->snooped, new, new);
    }

    scheduler_preempt_enable();
}

/**
 * This is set once before collection because currently new
 * threads will be created with a badly initialized tcb when
 * the collector is running, the real fix is to just have it
 * so the gc can set the default values for the tcb, but I am
 * too lazy to do it, so for now simply lock the amount of
 * threads we are going to scan, new threads will simply create
 * their objects as black, so it won't matter for us
 */
static int m_thread_count;

static void initiate_collection_cycle() {
    // first handshake
    lock_all_threads();
    m_thread_count = arrlen(g_all_threads);
    for (int i = 0; i < m_thread_count; i++) {
        thread_t* thread = g_all_threads[i];
        if (thread == get_current_thread()) continue;

        suspend_state_t state = scheduler_suspend_thread(thread);
        thread->tcb->gc_local_data.snoop = true;
        scheduler_resume_thread(state);
    }
    unlock_all_threads();

    // give a small room for new threads to be created...

    // second handshake
    lock_all_threads();
    for (int i = 0; i < m_thread_count; i++) {
        thread_t* thread = g_all_threads[i];
        if (thread == get_current_thread()) continue;

        suspend_state_t state = scheduler_suspend_thread(thread);
        thread->tcb->gc_local_data.trace_on = true;
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
    for (int i = 0; i < m_thread_count; i++) {
        thread_t* thread = g_all_threads[i];
        if (thread == get_current_thread()) continue;

        suspend_state_t state = scheduler_suspend_thread(thread);
        thread->tcb->gc_local_data.alloc_color = m_color_black;
        thread->tcb->gc_local_data.snoop = false;
        // TODO: copy thread local state...
        scheduler_resume_thread(state);
    }
    unlock_all_threads();

    // give a small room for new threads to be created...

    lock_all_threads();
    for (int i = 0; i < m_thread_count; i++) {
        thread_t* thread = g_all_threads[i];
        if (thread == get_current_thread()) continue;

        // copy and clear snooped objects set
        for (int j = 0; j < hmlen(thread->tcb->gc_local_data.snooped); j++) {
            object_t* o = thread->tcb->gc_local_data.snooped[j].key;
            hmput(m_roots, o, o);
        }
        hmfree(thread->tcb->gc_local_data.snooped);
    }
    unlock_all_threads();
}

static object_t** m_mark_stack = NULL;

static void trace(object_t* o) {
    if (o->color == m_color_white) {
        if (o->log_pointer == NULL) {
            // if not dirty

            // getting a replica
            size_t count = arrlen(o->type->managed_pointer_offsets);
            object_t* temp[count];
            for (int i = 0; i < count; i++) {
                temp[i] = read_field(o, o->type->managed_pointer_offsets[i]);
            }

            if (o->log_pointer == NULL) {
                for (int i = 0; i < count; i++) {
                    arrpush(m_mark_stack, temp[i]);
                }
            }
        } else {
            // object is dirty
            for (int i = 0; i < arrlen(o->type->managed_pointer_offsets); i++) {
                int ni = arrlen(o->type->managed_pointer_offsets) - i - 1;
                arrpush(m_mark_stack, o->log_pointer[ni]);
            }
        }

        o->color = m_color_black;
    }
}

static void trace_heap() {
    for (int i = 0; i < hmlen(m_roots); i++) {
        object_t* o = m_roots[i].key;
        arrpush(m_mark_stack, o);
    }

    while (arrlen(m_mark_stack) != 0) {
        object_t* o = arrpop(m_mark_stack);
        trace(o);
    }
}

static void sweep() {
    // fourth handshake
    lock_all_threads();
    for (int i = 0; i < m_thread_count; i++) {
        thread_t* thread = g_all_threads[i];
        if (thread == get_current_thread()) continue;
        suspend_state_t state = scheduler_suspend_thread(thread);
        thread->tcb->gc_local_data.trace_on = false;
        scheduler_resume_thread(state);
    }
    unlock_all_threads();

    // TODO: go over all objects in the heap

    spinlock_lock(&m_all_objects_lock);
    object_t* swept;
    object_t* temp;
    LIST_FOR_EACH_ENTRY_SAFE(swept, temp, &m_all_objects, link) {
        if (swept->color == m_color_white) {
            TRACE("Freeing %p", swept);
            list_del(&swept->link);
            free(swept);
        }
    }
    spinlock_unlock(&m_all_objects_lock);
}

static void prepare_next_collection() {
    hmfree(m_roots);
    lock_all_threads();
    for (int i = 0; i < m_thread_count; i++) {
        thread_t* thread = g_all_threads[i];
        if (thread == get_current_thread()) continue;

        // Clear all log pointers
        object_t** buffer = thread->tcb->gc_local_data.buffer;
        for (int j = 0; j < arrlen(buffer); j++) {
            buffer[j]->log_pointer = NULL;
        }

        // clear objects buffer
        arrfree(thread->tcb->gc_local_data.buffer);
    }
    unlock_all_threads();
}

static void gc_collection_cycle() {
    initiate_collection_cycle();
    get_roots();
    trace_heap();
    sweep();
    prepare_next_collection();
}

static bool m_running = true;
static mutex_t m_trigger_mutex;
static conditional_t m_wake;
static conditional_t m_done;

static void gc_next() {
    atomic_store(&m_running, false);
    conditional_broadcast(&m_done);
    do {
        conditional_wait(&m_wake, &m_trigger_mutex);
    } while (!atomic_load(&m_running));
}

void gc_wake() {
    if (atomic_load(&m_running)) {
        return;
    }
    atomic_store(&m_running, true);
    conditional_signal(&m_wake);
}

static void gc_wait_no_wake() {
    do {
        conditional_wait(&m_done, &m_trigger_mutex);
    } while (atomic_load(&m_running));
}

void gc_wait() {
    mutex_lock(&m_trigger_mutex);
    gc_wake();
    gc_wait_no_wake();
    mutex_unlock(&m_trigger_mutex);
}

noreturn static void gc_thread(void* ctx) {
    TRACE("GC thread started");

    while (true) {
        mutex_lock(&m_trigger_mutex);
        gc_next();
        mutex_unlock(&m_trigger_mutex);

        TRACE("Starting collection");
        gc_collection_cycle();
    }
}

err_t init_gc() {
    err_t err = NO_ERROR;

    thread_t* thread = create_thread(gc_thread, NULL, "kernel/gc");
    CHECK(thread != NULL);
    scheduler_ready_thread(thread);

    mutex_lock(&m_trigger_mutex);
    gc_wait_no_wake();
    mutex_unlock(&m_trigger_mutex);

cleanup:
    return err;
}
