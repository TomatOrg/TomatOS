#include "list.h"

#include <stddef.h>
#include <util/except.h>

void list_init(list_t *list) {
    list->prev = list;
    list->next = list;
}

static void list_add_internal(list_entry_t* prev, list_entry_t* next, list_entry_t* new) {
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

void list_add(list_t* head, list_entry_t* new) {
    list_add_internal(head, head->next, new);
}

void list_add_tail(list_t* head, list_entry_t* new) {
    list_add_internal(head->prev, head, new);
}

void list_del(list_entry_t* entry) {
    list_entry_t* prev = entry->prev;
    list_entry_t* next = entry->next;
    next->prev = prev;
    prev->next = next;
    entry->next = (list_entry_t*)0xAAAAAAAAAAAAAAAA;
    entry->prev = (list_entry_t*)0xBBBBBBBBBBBBBBBB;
}

bool list_is_empty(list_t* head) {
    return head->next == head;
}

list_entry_t* list_pop(list_t* head) {
    // check if we even have any
    if (list_is_empty(head)) {
        return NULL;
    }

    // take the last one and remove it
    list_entry_t* entry = head->prev;
    list_del(entry);
    return entry;
}
