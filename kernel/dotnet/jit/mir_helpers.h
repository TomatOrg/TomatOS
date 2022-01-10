#pragma once

#include "jitter_internal.h"

#include <mir/mir.h>

#define MIR_APPEND(after, insn) \
    ({ \
        MIR_insn_t __insn = (insn); \
        if (after == NULL) { \
            MIR_append_insn(ctx->ctx, ctx->func->func_item, __insn); \
        } else { \
            MIR_insert_insn_after(ctx->ctx, ctx->func->func_item, after, __insn); \
            after = __insn; \
        } \
    })

MIR_insn_t mir_emit_inline_memcpy(
    jitter_context_t* ctx,
    MIR_insn_t after,
    MIR_reg_t to_base, size_t to_offset,
    MIR_reg_t from_base, size_t from_offset,
    size_t size
);

MIR_insn_t mir_emit_inline_memset(
    jitter_context_t* ctx,
    MIR_insn_t after,
    MIR_reg_t to_base, size_t to_offset,
    uint8_t value8, size_t size
);

MIR_func_t mir_get_func(MIR_context_t ctx, const char* name);

MIR_item_t mir_get_forward(MIR_context_t ctx, const char* name);

MIR_proto_t mir_get_proto(MIR_context_t ctx, const char* name);

MIR_item_t mir_get_import(MIR_context_t ctx, const char* name);

MIR_bss_t mir_get_bss(MIR_context_t ctx, const char* name);
