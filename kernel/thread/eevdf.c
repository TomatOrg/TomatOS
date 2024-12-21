#include "eevdf.h"

#include <stddef.h>
#include <lib/except.h>

_Static_assert(offsetof(eevdf_node_t, node) == 0, "The node in eevdf_node must be first");

/**
 * The weights that are used for each priority level
 */
static uint8_t m_eevdf_priority_weight[EEVDF_PRIORITY_MAX] = {
    [EEVDF_PRIORITY_LOWEST] = 1,
    [EEVDF_PRIORITY_BELOW_NORMAL] = 2,
    [EEVDF_PRIORITY_NORMAL] = 3,
    [EEVDF_PRIORITY_ABOVE_NORMAL] = 4,
    [EEVDF_PRIORITY_HIGHEST] = 5,
};

/**
 * Get the lag of a node, assuming we are given the node's queue
 */
static int64_t eevdf_queue_get_lag(eevdf_queue_t* queue, eevdf_node_t* node) {
    return (queue->total_ideal_runtime[node->priority] - node->ideal_runtime_base) - node->runtime;
}

/**
 * Calculate the deadline of the thread
 */
static uint64_t eevdf_queue_calculate_virtual_deadline(eevdf_queue_t* queue, eevdf_node_t* node) {
    return queue->virtual_time + ((node->time_slice * queue->weights_sum) / m_eevdf_priority_weight[node->priority]);
}

static bool eevdf_heap_deadline_less_than(heap_node_t* _a, heap_node_t* _b, void* arg) {
    eevdf_node_t* a = (eevdf_node_t*)_a;
    eevdf_node_t* b = (eevdf_node_t*)_b;
    return a->virtual_deadline < b->virtual_deadline;
}

static bool eevdf_heap_lag_bigger_than(heap_node_t* _a, heap_node_t* _b, void* arg) {
    eevdf_node_t* a = (eevdf_node_t*)_a;
    eevdf_node_t* b = (eevdf_node_t*)_b;
    eevdf_queue_t* queue = arg;
    return eevdf_queue_get_lag(queue, a) > eevdf_queue_get_lag(queue, b);
}

/**
 * Insert the node to the eligible queue
 */
static void eevdf_queue_insert_eligible(eevdf_queue_t* queue, eevdf_node_t* node) {
    heap_insert(&queue->eligible, &node->node, eevdf_heap_deadline_less_than, NULL);
}

/**
 * Insert the node to the correct decaying loop
 */
static void eevdf_queue_insert_decaying(eevdf_queue_t* queue, eevdf_node_t* node) {
    heap_insert(&queue->decaying[node->priority], &node->node, eevdf_heap_lag_bigger_than, queue);
}

/**
 * Takes a node and makes it eligible, calculating the correct deadline
 * for it before inserting into the eligible queue
 */
static void eevdf_queue_make_eligible(eevdf_queue_t* queue, eevdf_node_t* node) {
    // Reset the ideal runtime base, the runtime value will have the
    // lag calculated into it already, making sure that we get the same
    // lag that we had beforehand
    node->ideal_runtime_base = queue->total_ideal_runtime[node->priority];

    // update the deadline again
    node->virtual_deadline = eevdf_queue_calculate_virtual_deadline(queue, node);

    // Insert to the eligible queue
    eevdf_queue_insert_eligible(queue, node);
}

void eevdf_queue_add(eevdf_queue_t* queue, eevdf_node_t* node) {
    ASSERT(node->queue == NULL);
    node->queue = queue;

    // update the total weights we have
    atomic_add(&queue->weights_sum, m_eevdf_priority_weight[node->priority]);

    // reset state just in case
    node->remove = false;
    node->runtime = 0;

    // And now make it eligible
    eevdf_queue_make_eligible(queue, node);
}

void eevdf_queue_wakeup(eevdf_queue_t* queue, eevdf_node_t* node) {
    ASSERT(node->queue == queue);

    if (node->remove) {
        // the node was still not removed from the
        // decaying list, so keep it there
        node->remove = false;

        // add back the weight now that we don't want it removed
        atomic_add(&queue->weights_sum, m_eevdf_priority_weight[node->priority]);

    } else {
        // we can only wakeup if the task was removed
        eevdf_queue_make_eligible(queue, node);
    }
}

/**
 * Check if any of the decaying nodes need to be removed form the decaying list
 * either to the eligible list or completely from the lists
 */
static void eevdf_queue_update_decaying(eevdf_queue_t* queue) {
    for (int i = 0; i < EEVDF_PRIORITY_MAX; i++) {
        heap_t* heap = &queue->decaying[i];

        // iterate the min nodes
        for (heap_node_t* node = heap_min_node(heap); node != NULL; node = heap_min_node(heap)) {
            // check if the node still has negative lag
            eevdf_node_t* enode = (eevdf_node_t*)node;
            int64_t lag = eevdf_queue_get_lag(queue, enode);
            if (lag < 0) {
                break;
            }

            // remove from the heap
            heap_pop(heap, eevdf_heap_lag_bigger_than, queue);

            // and now check what we need to do with the node
            if (enode->remove) {
                // the node is still not queued, reset its runtime
                // since it finished decaying
                enode->runtime = 0;

                // and mark that it does not need to be removed anymore
                enode->remove = false;
            } else {
                // the node is ready to be requeued, update
                // the deadline
                enode->virtual_deadline = eevdf_queue_calculate_virtual_deadline(queue, enode);

                // and queue on the eligible queue
                eevdf_queue_insert_eligible(queue, enode);
            }
        }
    }
}

static void eevdf_queue_tick(eevdf_queue_t* queue, int64_t time_slice) {
    // update the lag of each since last boot
    for (int i = 0; i < EEVDF_PRIORITY_MAX; i++) {
        queue->total_ideal_runtime[i] += (i * time_slice) / queue->weights_sum;
    }

    // update the virtual time of the queue
    queue->virtual_time += time_slice / queue->weights_sum;

    // update the decaying entries, remove anything if need be
    eevdf_queue_update_decaying(queue);
}

static void eevdf_queue_tick_current(eevdf_queue_t* queue, int64_t time_slice, bool requeue) {
    eevdf_node_t* current = queue->current;

    // make sure we have a current (in case we woke up from sleep)
    if (current == NULL) {
        return;
    }

    // update its runtime
    current->runtime += time_slice;

    int64_t lag = eevdf_queue_get_lag(queue, current);
    if (lag < 0) {
        // this now has a negative lag, so we need to
        // remove it from the queue

        if (!requeue) {
            // we don't want this to be requeud, mark for removal once
            // the decay is complete
            current->remove = true;

            // remove the weight
            atomic_sub(&queue->weights_sum, m_eevdf_priority_weight[current->priority]);
        }

        // Insert to the decaying loop until we are done
        eevdf_queue_insert_decaying(queue, current);

    } else if (requeue) {
        // update the deadline now that it is back to being eligible
        current->virtual_deadline = eevdf_queue_calculate_virtual_deadline(queue, current);

        // put back on the eligible queue
        eevdf_queue_insert_eligible(queue, current);

    } else {
        // This should not be re-queued, set the runtime to -lag
        // so that on next wakeup it will get the same lag as it
        // has right now
        current->runtime = -lag;
    }
}

static eevdf_node_t* eevdf_queue_choose_next(eevdf_queue_t* queue) {
    return (eevdf_node_t*)heap_pop(&queue->eligible, eevdf_heap_deadline_less_than, NULL);
}

eevdf_node_t* eevdf_queue_schedule(eevdf_queue_t* queue, int64_t time_slice, bool remove, bool requeue) {
    // if there are no weights then there are no threads
    // so just return
    if (queue->weights_sum == 0) {
        return NULL;
    }

    // tick the queue
    eevdf_queue_tick(queue, time_slice);

    // tick the current task if any
    if (!remove) {
        eevdf_queue_tick_current(queue, time_slice, requeue);
    }

    // and now choose the next node to run
    eevdf_node_t* node = eevdf_queue_choose_next(queue);
    queue->current = node;
    return node;
}

static void eevdf_queue_steal_one(eevdf_queue_t* queue, eevdf_queue_t* from, heap_t* heap, heap_is_less_func_t func, eevdf_node_t* node) {
    // remove from the given heap, we assume that it is actually at the
    // start of that heap always give the context just to make it easier
    heap_pop(heap, func, from);

    // the runtime will now be the -lag so the lag will be kept when moving it
    int64_t lag = eevdf_queue_get_lag(from, node);
    node->runtime = -lag;

    // set the ideal runtime base to the current one
    node->ideal_runtime_base = queue->total_ideal_runtime[node->priority];

    // adjust the deadline to be related to the new queue based
    // on the time remaining in the deadline
    node->virtual_deadline = queue->virtual_time + (node->virtual_deadline - from->virtual_time);

    // move the weight
    uint32_t weight = m_eevdf_priority_weight[node->priority];
    atomic_sub(&from->weights_sum, weight);
    atomic_add(&queue->weights_sum, weight);

    // and now insert it
    if (lag < 0) {
        // negative lag, insert to decaying heap
        heap_insert(heap, &node->node, eevdf_heap_lag_bigger_than, queue);
    } else {
        // positive lag, insert to the eligible heap
        eevdf_queue_insert_eligible(queue, node);
    }
}

static uint32_t eevdf_queue_steal_from_heap(eevdf_queue_t* queue, eevdf_queue_t* from, heap_t* heap, heap_is_less_func_t func, uint32_t max_weight) {
    // TODO: if the heap can support fast enough iteration we could maybe
    //       not stop on the first node we don't like but continue to iterate
    //       more
    uint32_t total_sum = 0;
    for (;;) {
        eevdf_node_t* node = (eevdf_node_t*)heap_min_node(&from->eligible);
        if (node == NULL) {
            return total_sum;
        }

        // if this is a node that needs to be removed then just
        // ignore the rest of the heap
        if (node->remove) {
            return total_sum;
        }

        // check if we are over the maximum sum
        total_sum += m_eevdf_priority_weight[node->priority];
        if (total_sum >= max_weight) {
            return total_sum;
        }

        // and steal it
        eevdf_queue_steal_one(queue, from, &from->eligible, eevdf_heap_deadline_less_than, node);
    }
}

void eevdf_queue_steal(eevdf_queue_t* queue, eevdf_queue_t* from, uint32_t max_weight) {
    // first attempt to steal from the eligible queue
    uint32_t total_sum = eevdf_queue_steal_from_heap(
        queue, from,
        &from->eligible, eevdf_heap_deadline_less_than,
        max_weight
    );

    // if we are still less than the desired weight attempt to
    // steal from the decaying loops, we will only steal decaying
    // items that have
    for (int i = EEVDF_PRIORITY_MAX - 1; i >= 0 && total_sum < max_weight; i--) {
        heap_t* heap = &from->decaying[i];
        total_sum += eevdf_queue_steal_from_heap(
            queue, from,
            heap, eevdf_heap_lag_bigger_than,
            max_weight - total_sum
        );
    }
}
