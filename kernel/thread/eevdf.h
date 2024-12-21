#pragma once

#include "lib/heap.h"
#include "lib/atomic.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum eevdf_priority {
    EEVDF_PRIORITY_LOWEST,
    EEVDF_PRIORITY_BELOW_NORMAL,
    EEVDF_PRIORITY_NORMAL,
    EEVDF_PRIORITY_ABOVE_NORMAL,
    EEVDF_PRIORITY_HIGHEST,
    EEVDF_PRIORITY_MAX
} eevdf_priority_t;

typedef struct eevdf_queue eevdf_queue_t;
typedef struct eevdf_node eevdf_node_t;

typedef struct eevdf_node {
    // the node in the heap this node is in
    heap_node_t node;

    // The queue this node is on
    eevdf_queue_t* queue;

    //
    // Configurable by the scheduler
    //

    // the priority of the node
    eevdf_priority_t priority;

    // the time slice the node needs/wants (physical time)
    uint32_t time_slice;

    //
    // Controlled by the eevdf queue
    //

    // when attached to an eevdf queue, this is the base lag that was at
    // the time of insertion to the queue
    int64_t ideal_runtime_base;

    // the physical runtime of the process, when not queued
    // in any queue this will have the -lag so that the next
    // lag calculations will return the same value as it had
    // before
    int64_t runtime;

    // the virtual deadline of the node
    uint64_t virtual_deadline;

    // if true then the node is decaying while being parked
    // once the lag becomes positive reset it
    // if false and we are parked then it means that once
    // the lag becomes positive we need to requeue it
    bool remove;
} eevdf_node_t;

typedef struct eevdf_queue {
    // the accumulated ideal runtime for each of the weights
    // in physical time
    int64_t total_ideal_runtime[EEVDF_PRIORITY_MAX];

    // the current virtual time of the queue
    int64_t virtual_time;

    // the total amount of weights we have in the queue
    _Atomic(uint32_t) weights_sum;

    // the currently running node
    eevdf_node_t* current;

    // the heap of eligible nodes, will only have nodes with a
    // positive lag value and is ordered by the deadlines
    heap_t eligible;

    // the decaying heaps, we separate it by weights, this allows
    // us to not touch anything in the order on each tick
    heap_t decaying[EEVDF_PRIORITY_MAX];
} eevdf_queue_t;

static inline uint32_t eevdf_queue_get_weights_sum(eevdf_queue_t* queue) {
    return atomic_read(&queue->weights_sum);
}

/**
 * Add a new thread into the queue
 *
 * @param queue         [IN] The queue to add to
 * @param node          [IN] The thread that we are adding
 */
void eevdf_queue_add(eevdf_queue_t* queue, eevdf_node_t* node);

/**
 * Wakeup a node, re-queueing it to the queue it was last on
 *
 * The only case where you want to wakeup is when the node that was
 * parked
 *
 * @param queue         [IN] The queue the node is on
 * @param node          [IN] The node that we need to requeue
 */
void eevdf_queue_wakeup(eevdf_queue_t* queue, eevdf_node_t* node);

/**
 * Schedule a node to run, will properly handle the current, if the last tick returned NULL
 * then this function should not be called until another thread either wakesup or gets added
 *
 * @param queue         [IN] The queue we are scheduling on
 * @param time_slice    [IN] The time since the last time schedule was called
 * @param remove        [IN] Should the current be removed
 * @param requeue       [IN] Should the current be requeued
 */
eevdf_node_t* eevdf_queue_schedule(eevdf_queue_t* queue, int64_t time_slice, bool remove, bool requeue);

/**
 * Steal an amount of nodes that is smaller or equals to the max weight requested.
 * Meant to be used to balance queues
 *
 * @param queue         [IN] The queue to steal to
 * @param from          [IN] The queue to steal from
 * @param max_weight    [IN] The max weight we allow to steal
 */
void eevdf_queue_steal(eevdf_queue_t* queue, eevdf_queue_t* from, uint32_t max_weight);
