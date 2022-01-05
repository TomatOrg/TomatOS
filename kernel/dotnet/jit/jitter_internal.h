#pragma once

#include <mir/mir.h>

typedef struct jitter_context {
    MIR_context_t ctx;
} jitter_context_t;

void random_mir_test();
