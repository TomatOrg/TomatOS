#pragma once

#include <mir/mir.h>

#include <dotnet/type.h>

typedef struct stack_item {
    type_t type;
    MIR_op_t op;
} stack_item_t;

typedef struct jit_stack {
    stack_item_t* stack;

    struct {
        uint32_t key;
        stack_item_t* value;
    }* stacks_by_cil;

    struct {
        uint32_t key;
        MIR_insn_t value;
    }* labels;

    // for integer types we just use normal registers
    int i;
    int max_i;

    // temp registers
    int temp;
    int max_temp;

    // for managed objects the pointer is stored on the stack so we
    // can scan for it when needed
    int o;
    int max_o;
    MIR_reg_t frame;
} jit_stack_t;

typedef struct jitter_context {
    MIR_context_t ctx;
    jit_stack_t stack;
    MIR_func_t func;

    // This allows to set the top of the stack for the
    // current frame
    MIR_item_t set_top_frame;
    MIR_item_t set_top_frame_proto;

    // This allows to allocate new objects
    MIR_item_t gc_new;
    MIR_item_t gc_new_proto;

    // This allows us to throw an exception
    MIR_item_t throw;
    MIR_item_t throw_proto;
} jitter_context_t;
