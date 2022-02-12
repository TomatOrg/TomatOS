#pragma once

#include "thread.h"

#include <arch/idt.h>

#include <stdbool.h>

bool scheduler_can_spin(int i);

void scheduler_ready_thread(thread_t* thread);

void scheduler_on_schedule(interrupt_context_t* ctx);

void scheduler_on_yield(interrupt_context_t* ctx);

void scheduler_on_park(interrupt_context_t* ctx);

void scheduler_on_startup(interrupt_context_t* ctx);

void scheduler_schedule();

void scheduler_yield();

void scheduler_park();

void scheduler_startup();

thread_t* get_current_thread();
