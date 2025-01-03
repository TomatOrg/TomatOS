#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <debug/log.h>

typedef struct list_entry {
    struct list_entry* next;
    struct list_entry* prev;
} list_entry_t;

typedef list_entry_t list_t;

#define LIST_INIT(head) \
    (list_entry_t){ .next = head, .prev = head }

#define containerof(ptr, type, member) \
    ((type*)((uint8_t*) (ptr) - offsetof(type, member)))

static inline void list_init(list_t* head) {
    head->next = head;
    head->prev = head;
}

static inline void __list_add(list_entry_t* new, list_entry_t* prev, list_entry_t* next) {
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

static inline void list_add(list_t* head, list_entry_t* new) {
    __list_add(new, head, head->next);
}

static inline void list_add_tail(list_t* head, list_entry_t* new) {
    __list_add(new, head->prev, head);
}

static inline void __list_del(list_entry_t* prev, list_entry_t* next) {
    next->prev = prev;
    prev->next = next;
}

static inline void list_del(list_entry_t* entry) {
    __list_del(entry->prev, entry->next);
    entry->next = (void*)0xdead000000000000;
    entry->prev = (void*)0xdead000010000000;
}

static inline bool list_is_empty(list_t* head) {
    return head->next == head;
}

static inline list_entry_t* list_pop(list_t* head) {
    if (list_is_empty(head)) {
        return NULL;
    }

    list_entry_t* entry = head->next;
    list_del(entry);
    return entry;
}
