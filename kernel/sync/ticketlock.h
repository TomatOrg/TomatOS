#pragma once

#include <stddef.h>
#include <stdbool.h>

typedef struct ticketlock {
    size_t next_ticket;
    size_t next_serving;
} ticketlock_t;

#define INIT_TICKETLOCK() ((ticketlock_t){ .next_ticket = 0, .next_serving = 0 })

void ticketlock_lock(ticketlock_t* lock);

void ticketlock_unlock(ticketlock_t* lock);

bool ticketlock_is_locked(ticketlock_t* lock);
