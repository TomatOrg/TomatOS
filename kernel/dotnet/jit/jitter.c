#include "jitter.h"

#include "jitter_internal.h"
#include "cil_opcode.h"
#include "mir_helpers.h"

#include <dotnet/method_info.h>
#include <dotnet/type.h>

#include <mir/mir.h>
#include <util/stb_ds.h>
#include <dotnet/types.h>

jitter_context_t* create_jitter() {
    jitter_context_t* jitter = malloc(sizeof(jitter_context_t));
    if (jitter == NULL) {
        return NULL;
    }

    jitter->ctx = MIR_init();
    return jitter;
}

#define FETCH(type) \
    ({ \
        CHECK(code_end - code >= sizeof(type)); \
        type __value = *((type*)code); \
        code += sizeof(type); \
        __value; \
    })
#define FETCH_I1() FETCH(int8_t)
#define FETCH_I4() FETCH(int32_t)
#define FETCH_I8() FETCH(int64_t)

#define STACK_FRAME_PREV_OFFSET         (0)
#define STACK_FRAME_METHOD_INFO_OFFSET  (STACK_FRAME_PREV_OFFSET + 8)
#define STACK_FRAME_OBJECT_COUNT_OFFSET (STACK_FRAME_METHOD_INFO_OFFSET + 8)
#define STACK_FRAME_OBJECTS_OFFSET      (STACK_FRAME_OBJECT_COUNT_OFFSET + 2)

static MIR_op_t jit_push(jitter_context_t* ctx, type_t type) {
    // TODO: queue type for jitting

    MIR_op_t op;
    if (type->is_primitive) {
        char temp_name[32] = {0 };
        snprintf(temp_name, sizeof(temp_name), "si%d", ctx->stack.i);

        MIR_reg_t reg;
        if (ctx->stack.i == ctx->stack.max_i) {
            // need new reg
            ctx->stack.max_i++;
            reg = MIR_new_func_reg(ctx->ctx, ctx->func, MIR_T_I64, temp_name);
        } else {
            // can reuse reg
            reg = MIR_reg(ctx->ctx, temp_name, ctx->func);
        }

        // increment for next one and create the reg operand
        op = MIR_new_reg_op(ctx->ctx, reg);
        ctx->stack.i++;

    } else if (!type->is_value_type) {
        if (ctx->stack.o == ctx->stack.max_o) {
            ctx->stack.max_o++;
        }
        op = MIR_new_mem_op(ctx->ctx, MIR_T_I64, STACK_FRAME_OBJECTS_OFFSET + ctx->stack.o * 8, ctx->stack.frame, 0, 0);
        ctx->stack.o++;
    } else {
        ASSERT(false);
    }

    stack_item_t item = {
        .type = type,
        .op = op
    };
    arrpush(ctx->stack.stack, item);

    return op;
}

static MIR_op_t jit_pop(jitter_context_t* ctx) {
    stack_item_t item = arrpop(ctx->stack.stack);

    if (item.type->is_primitive) {
        ctx->stack.i--;
    } else if (!item.type->is_value_type) {
        ctx->stack.o--;
    } else {
        ASSERT(false);
    }

    return item.op;
}

err_t jitter_jit_method(jitter_context_t* ctx, method_info_t method_info) {
    err_t err = NO_ERROR;

    // TODO: properly
    MIR_new_module(ctx->ctx, method_info->declaring_type->name);

    MIR_new_import(ctx->ctx, "method_info");

    ctx->func = MIR_new_func(ctx->ctx, method_info->name, 0, NULL, 0)->u.func;
    TRACE("%s.%s.%s", method_info->declaring_type->namespace, method_info->declaring_type->name, method_info->name);

    ctx->stack.frame = MIR_new_func_reg(ctx->ctx, ctx->func, MIR_T_I64, "stack_frame");

    // set to true if we have an instruction that could throw an exception
    bool might_throw_exception = false;

    uint8_t* code = method_info->il;
    uint8_t* code_end = code + method_info->il_size;
    do {
        // Fetch opcode, also handle the extended form
        uint16_t opcode = *code++;
        if (opcode == CIL_OPCODE_PREFIX1) {
            opcode <<= 8;
            opcode |= *code++;
        }

        TRACE("\t%s", cil_opcode_to_str(opcode));

        int32_t i4;
        switch (opcode) {
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // Base instructions
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////


            case CIL_OPCODE_DUP: {
                CHECK(arrlen(ctx->stack.stack) >= 1);
                stack_item_t src = arrlast(ctx->stack.stack);
                MIR_op_t dst = jit_push(ctx, src.type);
                MIR_append_insn(ctx->ctx, ctx->func->func_item,
                                MIR_new_insn(ctx->ctx, MIR_MOV, dst, src));
            } break;

            case CIL_OPCODE_LDC_I4_M1: i4 = -1; goto cil_opcode_ldc_i4;
            case CIL_OPCODE_LDC_I4_0: i4 = 0; goto cil_opcode_ldc_i4;
            case CIL_OPCODE_LDC_I4_1: i4 = 1; goto cil_opcode_ldc_i4;
            case CIL_OPCODE_LDC_I4_2: i4 = 2; goto cil_opcode_ldc_i4;
            case CIL_OPCODE_LDC_I4_3: i4 = 3; goto cil_opcode_ldc_i4;
            case CIL_OPCODE_LDC_I4_4: i4 = 4; goto cil_opcode_ldc_i4;
            case CIL_OPCODE_LDC_I4_5: i4 = 5; goto cil_opcode_ldc_i4;
            case CIL_OPCODE_LDC_I4_6: i4 = 6; goto cil_opcode_ldc_i4;
            case CIL_OPCODE_LDC_I4_7: i4 = 7; goto cil_opcode_ldc_i4;
            case CIL_OPCODE_LDC_I4_8: i4 = 8; goto cil_opcode_ldc_i4;
            case CIL_OPCODE_LDC_I4: i4 = FETCH_I4(); goto cil_opcode_ldc_i4;
            case CIL_OPCODE_LDC_I4_S: i4 = FETCH_I1(); goto cil_opcode_ldc_i4;
            cil_opcode_ldc_i4: {
                MIR_op_t dst = jit_push(ctx, g_int);
                MIR_append_insn(ctx->ctx, ctx->func->func_item,
                                MIR_new_insn(ctx->ctx, MIR_MOV, dst, MIR_new_int_op(ctx->ctx, i4)));
            } break;

            case CIL_OPCODE_LDC_I8: {
                int64_t i8 = FETCH_I8();
                MIR_op_t dst = jit_push(ctx, g_long);
                MIR_append_insn(ctx->ctx, ctx->func->func_item,
                                MIR_new_insn(ctx->ctx, MIR_MOV, dst, MIR_new_int_op(ctx->ctx, i8)));
            } break;

            case CIL_OPCODE_LDNULL: {
                // push a null value to the stack
                MIR_op_t op = jit_push(ctx, g_object);
                MIR_append_insn(ctx->ctx, ctx->func->func_item,
                                MIR_new_insn(ctx->ctx, MIR_MOV, op,
                                             MIR_new_int_op(ctx->ctx, 0)));
            } break;

            case CIL_OPCODE_NOP: {
                // do nothing
            } break;

            case CIL_OPCODE_POP: {
                jit_pop(ctx);
            } break;

            case CIL_OPCODE_RET: {
                if (method_info->return_type != NULL) {
                    // TODO: handle return types
                } else {
                    MIR_append_insn(ctx->ctx, ctx->func->func_item,
                                    MIR_new_ret_insn(ctx->ctx, 0));
                }
            } break;

            ////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // Object model instructions
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////


        }
    } while (code < code_end);

    // add the stack frame only if it is needed, we need it whenever we could have exceptions
    // (so we can have it in the stack trace) or when we actually have stack items that the gc
    // might need to scan.
    if (might_throw_exception || ctx->stack.max_o > 0) {
        // clear the objects
        MIR_insn_t stack_frame_setup = MIR_new_insn(ctx->ctx, MIR_ALLOCA,
                                                    MIR_new_reg_op(ctx->ctx, ctx->stack.frame),
                                                    MIR_new_int_op(ctx->ctx, STACK_FRAME_OBJECTS_OFFSET + ctx->stack.max_o * 8));
        MIR_prepend_insn(ctx->ctx, ctx->func->func_item, stack_frame_setup);

        // Set the method
        // TODO: serialize method name properly
        MIR_APPEND(stack_frame_setup, MIR_new_insn(ctx->ctx, MIR_MOV,
                                                   MIR_new_mem_op(ctx->ctx, MIR_T_I64,
                                                                  STACK_FRAME_METHOD_INFO_OFFSET, ctx->stack.frame, 0, 0),
                                                   MIR_new_ref_op(ctx->ctx, mir_get_import(ctx->ctx, "method_info"))));

        // setup the count
        MIR_APPEND(stack_frame_setup, MIR_new_insn(ctx->ctx, MIR_MOV,
                                                   MIR_new_mem_op(ctx->ctx, MIR_T_I16,
                                                                  STACK_FRAME_OBJECT_COUNT_OFFSET, ctx->stack.frame, 0, 0),
                                                   MIR_new_int_op(ctx->ctx, ctx->stack.max_o)));

        // zero out the whole stack frame
        mir_emit_inline_memset(ctx, stack_frame_setup,
                               ctx->stack.frame, STACK_FRAME_OBJECTS_OFFSET,
                               0x00, ctx->stack.max_o * 8);

        // TODO link it to the rest
    }

cleanup:
    MIR_finish_func(ctx->ctx);
    MIR_finish_module(ctx->ctx);

    arrfree(ctx->stack.stack);
    memset(&ctx->stack, 0, sizeof(ctx->stack));

    return err;
}

void destroy_jitter(jitter_context_t* jitter) {
    if (jitter == NULL) return;

    if (jitter->ctx != NULL) {
        buffer_t* buffer = create_buffer();
        MIR_output(jitter->ctx, buffer);
        printf("%.*s", arrlen(buffer->buffer), buffer->buffer);
        destroy_buffer(buffer);

        MIR_finish(jitter->ctx);
    }
    free(jitter);
}

