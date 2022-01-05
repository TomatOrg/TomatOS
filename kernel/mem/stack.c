#include "stack.h"

#include "mem.h"

#include <sync/ticketlock.h>
#include <util/string.h>

/**
 * Protects the stack allocator
 */
static ticketlock_t m_stack_alloc_lock = INIT_TICKETLOCK();

/**
 * This points to the next stack that we can allocate
 */
static void* m_next_stack = (void*)STACK_POOL_START;

/**
 * A list of free stacks
 */
static list_t m_stack_free_list = INIT_LIST(m_stack_free_list);

void* alloc_stack() {
    void* ret = NULL;

    ticketlock_lock(&m_stack_alloc_lock);

    list_entry_t* stack = list_pop(&m_stack_free_list);
    if (stack != NULL) {
        // we got a stack from the cache, we need to get the end of the
        // list entry struct to get the actual base of the stack
        ret = (void*)(stack + 1);
        goto cleanup;
    }

    // we need to allocate a new one, check that we have space for that
    if (((uintptr_t)m_next_stack + (SIZE_1MB * 3)) >= STACK_POOL_END) {
        goto cleanup;
    }

    // get the next entry
    ret = m_next_stack;

    // increment it by 3mb to account for guard page
    // and the stack space
    m_next_stack += SIZE_1MB * 3;

    // move by 2mb to get to the base of the stack
    ret += SIZE_2MB;

cleanup:
    ticketlock_unlock(&m_stack_alloc_lock);

    if (ret != NULL) {
        // access the first page just so we can
        // make sure it has anything
        memset(ret - 1, 0, 1);
    }

    return ret;
}

void free_stack(void* stack) {
    ticketlock_lock(&m_stack_alloc_lock);

    // get the entry from the end of the stack
    list_entry_t* entry = (list_entry_t*)stack - 1;
    list_push(&m_stack_free_list, entry);

    ticketlock_unlock(&m_stack_alloc_lock);
}
