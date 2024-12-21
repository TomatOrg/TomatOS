#include "heap.h"

void heap_insert(heap_t* heap, heap_node_t* node, heap_is_less_func_t is_less, void* ctx) {
    // initialize the heap if need be
    if (heap->root.next == NULL) {
        list_init(&heap->root);
    }

    list_entry_t* link = NULL;
    for (link = heap->root.next; link != &heap->root; link = link->next) {
        heap_node_t* node2 = containerof(link, heap_node_t, entry);
        if (is_less(node, node2, ctx)) {
            break;
        }
    }

    list_add_tail(link, &node->entry);
}

heap_node_t* heap_min_node(heap_t* heap) {
    // initialize the heap if need be
    if (heap->root.next == NULL) {
        list_init(&heap->root);
    }

    if (list_is_empty(&heap->root)) {
        return NULL;
    }
    return containerof(heap->root.next, heap_node_t, entry);
}

heap_node_t* heap_pop(heap_t* heap, heap_is_less_func_t is_less, void* ctx) {
    heap_node_t* node = heap_min_node(heap);
    if (node != NULL) {
        list_del(&node->entry);
    }
    return node;
}
