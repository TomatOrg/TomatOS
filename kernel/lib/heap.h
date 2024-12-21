#pragma once

#include <stddef.h>
#include <lib/list.h>

typedef struct heap_node heap_node_t;

struct heap_node {
    list_entry_t entry;
};

typedef struct heap {
    list_t root;
} heap_t;

/**
 * checks if a < b
 */
typedef bool (*heap_is_less_func_t)(heap_node_t* a, heap_node_t* b, void* arg);

/**
 * Insert a new node into the heap,
 */
void heap_insert(heap_t* heap, heap_node_t* node, heap_is_less_func_t is_less, void* arg);

/**
 * Returns true if the heap is empty
 */
bool heap_is_empty(heap_t* heap);

/**
 * Get the minimum node of the heap
 */
heap_node_t* heap_min_node(heap_t* heap);

/**
 * Pop the minimum node from the list
 */
heap_node_t* heap_pop(heap_t* heap, heap_is_less_func_t is_less, void* arg);
