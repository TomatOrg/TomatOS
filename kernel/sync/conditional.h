#pragma once

#include "mutex.h"
#include "notify_list.h"

typedef struct conditional {
    notify_list_t notify;
} conditional_t;

void conditional_wait(conditional_t* conditional, mutex_t* mutex);

void conditional_signal(conditional_t* conditional);

void conditional_broadcast(conditional_t* conditional);
