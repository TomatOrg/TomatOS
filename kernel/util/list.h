#pragma once

#include <stdbool.h>

#define LIST_ENTRY(ptr, type, member) ((type*)((char*)(ptr) - (char*)offsetof(type, member)))

#define LIST_FIRST_ENTRY(ptr, type, member) LIST_ENTRY((ptr)->next, type, member)
#define LIST_LAST_ENTRY(ptr, type, member) LIST_ENTRY((ptr)->prev, type, member)
#define LIST_NEXT_ENTRY(pos, member) LIST_ENTRY((pos)->member.next, typeof(*(pos)), member)
#define LIST_PREV_ENTRY(pos, member) LIST_ENTRY((pos)->member.prev, typeof(*(pos)), member)

#define LIST_ENTRY_IS_HEAD(pos, head, member) (&pos->member == (head))

#define LIST_FOR_EACH_ENTRY(pos, head, member) \
	for (pos = LIST_FIRST_ENTRY(head, typeof(*pos), member); !LIST_ENTRY_IS_HEAD(pos, head, member); pos = LIST_NEXT_ENTRY(pos, member))


/**
 * an entry in a list
 */
typedef struct list_entry {
    struct list_entry* next;
    struct list_entry* prev;
} list_entry_t;

/**
 * The head of a list
 */
typedef list_entry_t list_t;

#define INIT_LIST(var) (list_t){ &var, &var }

void list_init(list_t* list);

void list_add(list_t* head, list_entry_t* new);

void list_add_tail(list_t* head, list_entry_t* prev);

void list_del(list_entry_t* entry);

bool list_is_empty(list_t* head);

list_entry_t* list_pop(list_t* head);
