#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

static inline void list_add(list_t* head, list_entry_t* entry) {
    entry->next = head->next;
    entry->prev = head;
    entry->next->prev = entry;
    head->next = entry;
}

static inline void list_add_tail(list_t* head, list_entry_t* entry) {
    entry->next = head;
    entry->prev = head->prev;
    entry->prev->next = entry;
    head->prev = entry;
}

static inline void list_del(list_entry_t* entry) {
    entry->next->prev = entry->prev;
    entry->prev->next = entry->next;
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
