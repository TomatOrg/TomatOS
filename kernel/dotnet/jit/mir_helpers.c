#include "mir_helpers.h"

#include "jitter.h"
#include "jitter_internal.h"

#include <mir/mir.h>
#include <util/string.h>

MIR_insn_t mir_emit_inline_memcpy(
    jitter_context_t* ctx,
    MIR_insn_t after,
    MIR_reg_t to_base, size_t to_offset,
    MIR_reg_t from_base, size_t from_offset,
    size_t size
) {
    size_t size_left = size;
    while (size_left >= 8) {
        MIR_APPEND(after, MIR_new_insn(ctx->ctx, MIR_MOV,
                                       MIR_new_mem_op(ctx->ctx, MIR_T_I64,
                                                  to_offset + size - size_left,
                                                  to_base, 0, 1),
                                       MIR_new_mem_op(ctx->ctx, MIR_T_I64,
                                                  from_offset + size - size_left,
                                                  from_base, 0, 1)));
        size_left -= 8;
    }
    if (size_left >= 4) {
        MIR_APPEND(after, MIR_new_insn(ctx->ctx, MIR_MOV,
                                       MIR_new_mem_op(ctx->ctx, MIR_T_I32,
                                                    to_offset + size - size_left,
                                                    to_base, 0, 1),
                                       MIR_new_mem_op(ctx->ctx, MIR_T_I32,
                                                    from_offset + size - size_left,
                                                    from_base, 0, 1)));
        size_left -= 4;
    }
    if (size_left >= 2) {
        MIR_APPEND(after, MIR_new_insn(ctx->ctx, MIR_MOV,
                                       MIR_new_mem_op(ctx->ctx, MIR_T_I16,
                                                    to_offset + size - size_left,
                                                    to_base, 0, 1),
                                       MIR_new_mem_op(ctx->ctx, MIR_T_I16,
                                                    from_offset + size - size_left,
                                                    from_base, 0, 1)));
        size_left -= 2;
    }
    if (size_left >= 1) {
        MIR_APPEND(after, MIR_new_insn(ctx->ctx, MIR_MOV,
                                       MIR_new_mem_op(ctx->ctx, MIR_T_I8,
                                                    to_offset + size - size_left,
                                                    to_base, 0, 1),
                                       MIR_new_mem_op(ctx->ctx, MIR_T_I8,
                                                    from_offset + size - size_left,
                                                    from_base, 0, 1)));
        size_left -= 1;
    }
    return after;
}

MIR_insn_t mir_emit_inline_memset(
    jitter_context_t* ctx,
    MIR_insn_t after,
    MIR_reg_t to_base, size_t to_offset,
    uint8_t value8, size_t size
) {
    uint16_t value16 = ((uint16_t)value8) | ((uint16_t)value8 << 8);
    uint32_t value32 = ((uint32_t)value16) | ((uint32_t)value16 << 16);
    uint64_t value64 = ((uint64_t)value32) | ((uint64_t)value32 << 32);
    size_t size_left = size;
    while (size_left >= 8) {
        MIR_APPEND(after, MIR_new_insn(ctx->ctx, MIR_MOV,
                                       MIR_new_mem_op(ctx->ctx, MIR_T_I64,
                                                    to_offset + size - size_left,
                                                    to_base, 0, 1),
                                       MIR_new_uint_op(ctx->ctx, value64)));
        size_left -= 8;
    }
    if (size_left >= 4) {
        MIR_APPEND(after, MIR_new_insn(ctx->ctx, MIR_MOV,
                                       MIR_new_mem_op(ctx->ctx, MIR_T_I32,
                                                    to_offset + size - size_left,
                                                    to_base, 0, 1),
                                       MIR_new_uint_op(ctx->ctx, value32)));
        size_left -= 4;
    }
    if (size_left >= 2) {
        MIR_APPEND(after, MIR_new_insn(ctx->ctx, MIR_MOV,
                                       MIR_new_mem_op(ctx->ctx, MIR_T_I16,
                                                    to_offset + size - size_left,
                                                    to_base, 0, 1),
                                       MIR_new_uint_op(ctx->ctx, value16)));
        size_left -= 2;
    }
    if (size_left >= 1) {
        MIR_APPEND(after, MIR_new_insn(ctx->ctx, MIR_MOV,
                                       MIR_new_mem_op(ctx->ctx, MIR_T_I8,
                                                    to_offset + size - size_left,
                                                    to_base, 0, 1),
                                       MIR_new_uint_op(ctx->ctx, value8)));
        size_left -= 1;
    }
    return after;
}

MIR_item_t mir_get_data(MIR_context_t ctx, const char* name) {
    DLIST(MIR_module_t)* modules = MIR_get_module_list(ctx);
    for (MIR_module_t module = DLIST_HEAD (MIR_module_t, *modules); module != NULL; module = DLIST_NEXT (MIR_module_t, module)) {
        for (MIR_item_t item = DLIST_HEAD (MIR_item_t, module->items); item != NULL; item = DLIST_NEXT (MIR_item_t, item)) {
            if (item->item_type == MIR_data_item && strcmp(item->u.data->name, name) == 0) {
                return item;
            }
        }
    }
    return NULL;
}

MIR_func_t mir_get_func(MIR_context_t ctx, const char* name) {
    DLIST(MIR_module_t)* modules = MIR_get_module_list(ctx);
    for (MIR_module_t module = DLIST_HEAD (MIR_module_t, *modules); module != NULL; module = DLIST_NEXT (MIR_module_t, module)) {
        for (MIR_item_t item = DLIST_HEAD (MIR_item_t, module->items); item != NULL; item = DLIST_NEXT (MIR_item_t, item)) {
            if (item->item_type == MIR_func_item && strcmp(item->u.func->name, name) == 0) {
                return item->u.func;
            }
        }
    }
    return NULL;
}

MIR_item_t mir_get_forward(MIR_context_t ctx, const char* name) {
    DLIST(MIR_module_t)* modules = MIR_get_module_list(ctx);
    for (MIR_module_t module = DLIST_HEAD (MIR_module_t, *modules); module != NULL; module = DLIST_NEXT (MIR_module_t, module)) {
        for (MIR_item_t item = DLIST_HEAD (MIR_item_t, module->items); item != NULL; item = DLIST_NEXT (MIR_item_t, item)) {
            if (item->item_type == MIR_forward_item && strcmp(item->u.forward_id, name) == 0) {
                return item;
            }
        }
    }
    return NULL;
}

MIR_proto_t mir_get_proto(MIR_context_t ctx, const char* name) {
    DLIST(MIR_module_t)* modules = MIR_get_module_list(ctx);
    for (MIR_module_t module = DLIST_HEAD (MIR_module_t, *modules); module != NULL; module = DLIST_NEXT (MIR_module_t, module)) {
        for (MIR_item_t item = DLIST_HEAD (MIR_item_t, module->items); item != NULL; item = DLIST_NEXT (MIR_item_t, item)) {
            if (item->item_type == MIR_proto_item && strcmp(item->u.proto->name, name) == 0) {
                return item->u.proto;
            }
        }
    }
    return NULL;
}

MIR_item_t mir_get_import(MIR_context_t ctx, const char* name) {
    DLIST(MIR_module_t)* modules = MIR_get_module_list(ctx);
    for (MIR_module_t module = DLIST_HEAD (MIR_module_t, *modules); module != NULL; module = DLIST_NEXT (MIR_module_t, module)) {
        for (MIR_item_t item = DLIST_HEAD (MIR_item_t, module->items); item != NULL; item = DLIST_NEXT (MIR_item_t, item)) {
            if (item->item_type == MIR_import_item && strcmp(item->u.import_id, name) == 0) {
                return item;
            }
        }
    }
    return NULL;
}

MIR_bss_t mir_get_bss(MIR_context_t ctx, const char* name) {
    DLIST(MIR_module_t)* modules = MIR_get_module_list(ctx);
    for (MIR_module_t module = DLIST_HEAD (MIR_module_t, *modules); module != NULL; module = DLIST_NEXT (MIR_module_t, module)) {
        for (MIR_item_t item = DLIST_HEAD (MIR_item_t, module->items); item != NULL; item = DLIST_NEXT (MIR_item_t, item)) {
            if (item->item_type == MIR_bss_item && strcmp(item->u.bss->name, name) == 0) {
                return item->u.bss;
            }
        }
    }
    return NULL;
}
