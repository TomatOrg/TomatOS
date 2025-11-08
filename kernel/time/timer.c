#include "timer.h"

#include "tsc.h"
#include "arch/apic.h"
#include "lib/rbtree/rbtree.h"
#include "sync/spinlock.h"
#include "thread/pcpu.h"
#include "thread/scheduler.h"

typedef struct timer_backend {
    void (*set_deadline)(uint64_t tsc_deadline);
    void (*clear)(void);
} timer_backend_t;

/**
 * The timer we are using
 */
static timer_backend_t m_timer_backend = {};

struct per_core_timers {
    // the tree of timers, we use cached to have a quick
    // access to the min node
    rb_root_cached_t tree;
};

/**
 * The local timers for the core
 */
static CPU_LOCAL per_core_timers_t m_timers = {
    .tree = RB_ROOT_CACHED
};

void init_timers(void) {
    if (tsc_deadline_is_supported()) {
        TRACE("timer: using TSC deadline");
        m_timer_backend.set_deadline = tsc_timer_set_deadline;
        m_timer_backend.clear = tsc_timer_clear;
    } else {
        TRACE("timer: using APIC timer");
        m_timer_backend.set_deadline = lapic_timer_set_deadline;
        m_timer_backend.clear = lapic_timer_clear;
    }
}

static bool timer_less(rb_node_t* a, const rb_node_t* b) {
    timer_t* ta = containerof(a, timer_t, node);
    timer_t* tb = containerof(b, timer_t, node);
    return ta->deadline < tb->deadline;
}

void timer_set(timer_t* timer, timer_callback_t callback, uint64_t tsc_deadline) {
    // ensure the timer is canceled first
    timer_cancel(timer);

    bool irq_state = irq_save();
    timer->deadline = tsc_deadline;
    timer->callback = callback;
    timer->timers = pcpu_get_pointer(&m_timers);

    if (rb_add_cached(&timer->node, pcpu_get_pointer(&m_timers.tree), timer_less) != NULL) {
        // if we are the new leftmost node then we are the next timer to arrive,
        // so set the deadline to us
        m_timer_backend.set_deadline(tsc_deadline);
    }

    irq_restore(irq_state);
}

void timer_cancel(timer_t* timer) {
    per_core_timers_t* timers = timer->timers;
    if (timers == NULL) {
        return;
    }
    ASSERT(timers == pcpu_get_pointer(&m_timers));
    timer->timers = NULL;

    bool irq_state = irq_save();

    rb_node_t* old_leftmost = rb_first_cached(&timers->tree);
    rb_node_t* new_leftmost = rb_erase_cached(&timer->node, &timers->tree);
    if (old_leftmost != new_leftmost) {
        // if the left most node changed it means that we were the left most node
        // and that the next timer should arrive later, update the timeout
        if (new_leftmost != NULL) {
            uint64_t new_deadline = containerof(new_leftmost, timer_t, node)->deadline;
            m_timer_backend.set_deadline(new_deadline);
        } else {
            // no more timers on the core, we can clear the timer
            m_timer_backend.clear();
        }
    }

    irq_restore(irq_state);
}


void timer_dispatch(void) {
    // go over the timers in the tree that should be executed right now
    bool irq_state = irq_save();
    timer_t* timer = NULL;
    for (;;) {
        rb_node_t* node = rb_first_cached(&m_timers.tree);
        if (node == NULL) {
            timer = NULL;
            break;
        }

        timer = containerof(node, timer_t, node);
        if (get_tsc() < timer->deadline) {
            break;
        }

        // remove from the tree
        ASSERT(timer->timers == pcpu_get_pointer(&m_timers));
        rb_erase_cached(&timer->node, pcpu_get_pointer(&m_timers.tree));
        timer->timers = NULL;

        // we are done, we can unlock it
        irq_restore(irq_state);

        // call the callback, this may modify the tree however it wants
        // to and even have an earlier timer because we will just iterate
        // again and get the first one again
        timer->callback(timer);

        // we don't need to save again, we can just disable
        // since we have a known irq state
        irq_disable();
    }

    // if we still have a timer object in here it means that this is the next
    // time we should run it, setup the timer
    if (timer != NULL) {
        m_timer_backend.set_deadline(timer->deadline);
    } else {
        m_timer_backend.clear();
    }

    // we are done, its safe to do stuff again
    irq_restore(irq_state);
}

typedef struct sleep_ctx {
    timer_t timer;
    uint64_t ms_timeout;
    thread_t* thread;
} sleep_ctx_t;

static void sleep_wakeup_thread(timer_t* ctx) {
    sleep_ctx_t* sleep = containerof(ctx, sleep_ctx_t, timer);
    scheduler_wakeup_thread(sleep->thread);
}

static bool sleep_park_callback(void* ctx) {
    sleep_ctx_t* sleep = ctx;
    timer_set(&sleep->timer, sleep_wakeup_thread, tsc_ms_deadline(sleep->ms_timeout));
    return true;
}

void timer_sleep(uint64_t ms) {
    sleep_ctx_t ctx = {
        .thread = scheduler_get_current_thread(),
        .ms_timeout = ms,
    };
    scheduler_park(sleep_park_callback, &ctx);
}
