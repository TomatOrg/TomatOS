#include "jit.h"

#include "runtime/dotnet/opcodes.h"
#include "util/stb_ds.h"
#include "mir/mir.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Type helpers
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static bool is_type_integer(System_Type type) {
    return type == tSystem_Byte || type == tSystem_Int16 || type == tSystem_Int32 || type == tSystem_Int64 ||
           type == tSystem_SByte || type == tSystem_UInt16 || type == tSystem_UInt32 || type == tSystem_UInt64 ||
           type == tSystem_UIntPtr || type == tSystem_IntPtr || type == tSystem_Char || type == tSystem_Boolean;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The context of the jit
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct stack_entry {
    // the type of the stack entry
    System_Type type;

    // the location where this value
    // is stored on the stack
    MIR_op_t op;
} stack_entry_t;

typedef struct stack {
    // the stack entries
    stack_entry_t* entries;
} stack_t;

typedef struct jit_context {
    // stores all the different possible stack states
    stack_t* stack_splits;
    struct {
        int key; // the pc of the stack
        int value; // the index to the stack splits to get the correct stack
        int depth; // the depth of the stack split at this point
    }* pc_to_stack_split;

    // the index to the current stack
    int current_stack;

    // the mir context relating to this stack
    MIR_context_t context;

    // the function that this stack is for

    MIR_item_t func;

    // the current method being compiled
    System_Reflection_MethodInfo method_info;

    int name_gen;
} jit_context_t;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Stack helpers
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static err_t stack_push(jit_context_t* ctx, System_Type type, MIR_op_t* op) {
    err_t err = NO_ERROR;

    stack_entry_t entry = {
        .type = type
    };

    // create the name
    char name[64];
    snprintf(name, sizeof(name), "stack_%d", ++ctx->name_gen);

    // Make sure we don't exceed the stack depth
    CHECK (ctx->name_gen <= ctx->method_info->MethodBody->MaxStackSize);

    // create the reg
    MIR_reg_t reg;
    if (is_type_integer(type) || !type->IsValueType) {
        // This is an integer or a reference type
        reg = MIR_new_func_reg(ctx->context, ctx->func->u.func, MIR_T_I64, name);
    } else if (type == tSystem_Single) {
        ASSERT(FALSE);
        // This is a float
        reg = MIR_new_func_reg(ctx->context, ctx->func->u.func, MIR_T_F, name);
    } else if (type == tSystem_Double) {
        ASSERT(FALSE);
        // This is a double
        reg = MIR_new_func_reg(ctx->context, ctx->func->u.func, MIR_T_D, name);
    } else {
        ASSERT(FALSE);
        // This is a value type, allocate a big enough space for it at the start
        reg = MIR_new_func_reg(ctx->context, ctx->func->u.func, MIR_T_I64, name);
        MIR_prepend_insn(ctx->context, ctx->func,
                         MIR_new_insn(ctx->context, MIR_ALLOCA,
                                      MIR_new_reg_op(ctx->context, reg),
                                      MIR_new_int_op(ctx->context, type->StackSize)));
    }

    // set the actual op
    entry.op = MIR_new_reg_op(ctx->context, reg);

    // give out if needed
    if (op != NULL) {
        *op = entry.op;
    }

    // append to the stack
    arrpush(ctx->stack_splits[ctx->current_stack].entries, entry);

cleanup:
    return err;
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "UnreachableCode"
err_t jit_method(System_Reflection_MethodInfo method) {
    err_t err = NO_ERROR;

    System_Reflection_MethodBody body = method->MethodBody;
    System_Reflection_Assembly assembly = method->Module->Assembly;

    jit_context_t ctx = {
        .method_info = method
    };

    // we need at least one entry
    arrsetlen(ctx.stack_splits, 1);

    ctx.context = MIR_init();
    MIR_new_module(ctx.context, "temp");
    ctx.func = MIR_new_func(ctx.context, "temp", 0, NULL, 0);

    int il_offset = 0;
    while (il_offset < body->Il->Length) {
        uint16_t opcode_value = (REFPRE << 8) | body->Il->Data[il_offset++];

        // get the actual opcode
        opcode_t opcode = g_dotnet_opcode_lookup[opcode_value];
        CHECK_ERROR(opcode != CEE_INVALID, ERROR_INVALID_OPCODE);

        if (
            opcode == CEE_PREFIX1 ||
            opcode == CEE_PREFIX2 ||
            opcode == CEE_PREFIX3 ||
            opcode == CEE_PREFIX4 ||
            opcode == CEE_PREFIX5 ||
            opcode == CEE_PREFIX6 ||
            opcode == CEE_PREFIX7
        ) {
            opcode_info_t* opcode_info = &g_dotnet_opcodes[opcode];

            // setup the new prefix
            opcode_value <<= 8;
            opcode_value |= body->Il->Data[il_offset++];
            opcode = g_dotnet_opcode_lookup[opcode_value];
            CHECK_ERROR(opcode != CEE_INVALID, ERROR_INVALID_OPCODE);
        }

        // get the actual opcode
        opcode_info_t* opcode_info = &g_dotnet_opcodes[opcode];

        //--------------------------------------------------------------------------------------------------------------
        // Inline operands
        //--------------------------------------------------------------------------------------------------------------

        int32_t operand_i32;
        int64_t operand_i64;
        System_Reflection_FieldInfo operand_field;
        System_Reflection_MethodInfo operand_method;
        float operand_f32;
        double operand_f64;
        System_Type operand_type;

        char param[128] = { 0 };
        switch (opcode_info->operand) {
            case OPCODE_OPERAND_InlineBrTarget: {
                operand_i32 = il_offset + *(int32_t*)&body->Il->Data[il_offset];
                il_offset += sizeof(int32_t);
            } break;
            case OPCODE_OPERAND_InlineField: {
                token_t value = *(token_t*)&body->Il->Data[il_offset];
                il_offset += sizeof(token_t);
                operand_field= assembly_get_field_by_token(assembly, value);
            } break;
            case OPCODE_OPERAND_InlineI: {
                operand_i32 = *(int32_t*)&body->Il->Data[il_offset];
                il_offset += sizeof(int32_t);
            } break;
            case OPCODE_OPERAND_InlineI8: {
                operand_i64 = *(int64_t*)&body->Il->Data[il_offset];
                il_offset += sizeof(int64_t);
            } break;
            case OPCODE_OPERAND_InlineMethod: {
                token_t value = *(token_t*)&body->Il->Data[il_offset];
                il_offset += sizeof(token_t);
                operand_method = assembly_get_method_by_token(assembly, value);
            } break;
            case OPCODE_OPERAND_InlineR: {
                operand_f64 = *(double*)&body->Il->Data[il_offset];
                il_offset += sizeof(double);
            } break;
            case OPCODE_OPERAND_InlineSig: CHECK_FAIL("TODO: sig support");; break;
            case OPCODE_OPERAND_InlineString: CHECK_FAIL("TODO: string support"); break;
            case OPCODE_OPERAND_InlineSwitch: CHECK_FAIL("TODO: switch support");
            case OPCODE_OPERAND_InlineTok: CHECK_FAIL("TODO: tok support");; break;
            case OPCODE_OPERAND_InlineType: {
                token_t value = *(token_t*)&body->Il->Data[il_offset];
                il_offset += sizeof(token_t);
                operand_type = assembly_get_type_by_token(assembly, value);
            } break;
            case OPCODE_OPERAND_InlineVar: {
                operand_i32 = *(uint16_t*)&body->Il->Data[il_offset];
                il_offset += sizeof(uint16_t);
            } break;
            case OPCODE_OPERAND_ShortInlineBrTarget: {
                operand_i32 = il_offset + *(int8_t*)&body->Il->Data[il_offset];
                il_offset += sizeof(int8_t);
            } break;
            case OPCODE_OPERAND_ShortInlineI: {
                operand_i32 = *(int8_t*)&body->Il->Data[il_offset];
                il_offset += sizeof(int8_t);
            } break;
            case OPCODE_OPERAND_ShortInlineR: {
                operand_f32 = *(float*)&body->Il->Data[il_offset];
                il_offset += sizeof(float);
            } break;
            case OPCODE_OPERAND_ShortInlineVar: {
                operand_i32 = *(uint8_t*)&body->Il->Data[il_offset];
                il_offset += sizeof(uint8_t);
            } break;
            default: break;
        }

        //--------------------------------------------------------------------------------------------------------------
        // Pop from the stack (in a generic way for simplicity)
        //--------------------------------------------------------------------------------------------------------------

        switch (opcode_info->pop) {
            case OPCODE_STACK_BEHAVIOUR_Pop0: break;
            case OPCODE_STACK_BEHAVIOUR_Pop1: break;
            case OPCODE_STACK_BEHAVIOUR_Pop1_Pop1: break;
            case OPCODE_STACK_BEHAVIOUR_PopI: break;
            case OPCODE_STACK_BEHAVIOUR_PopI_Pop1: break;
            case OPCODE_STACK_BEHAVIOUR_PopI_PopI: break;
            case OPCODE_STACK_BEHAVIOUR_PopI_PopI8: break;
            case OPCODE_STACK_BEHAVIOUR_PopI_PopI_PopI: break;
            case OPCODE_STACK_BEHAVIOUR_PopI8_Pop8: break;
            case OPCODE_STACK_BEHAVIOUR_PopI_PopR4: break;
            case OPCODE_STACK_BEHAVIOUR_PopI_PopR8: break;
            case OPCODE_STACK_BEHAVIOUR_PopRef: break;
            case OPCODE_STACK_BEHAVIOUR_PopRef_Pop1: break;
            case OPCODE_STACK_BEHAVIOUR_PopRef_PopI: break;
            case OPCODE_STACK_BEHAVIOUR_PopRef_PopI_Pop1: break;
            case OPCODE_STACK_BEHAVIOUR_PopRef_PopI_PopI: break;
            case OPCODE_STACK_BEHAVIOUR_PopRef_PopI_PopI8: break;
            case OPCODE_STACK_BEHAVIOUR_PopRef_PopI_PopR4: break;
            case OPCODE_STACK_BEHAVIOUR_PopRef_PopI_PopR8: break;
            case OPCODE_STACK_BEHAVIOUR_PopRef_PopI_PopRef: break;
            case OPCODE_STACK_BEHAVIOUR_VarPop: break;
        }

        //--------------------------------------------------------------------------------------------------------------
        // Handle the opcode
        //--------------------------------------------------------------------------------------------------------------

        switch (opcode) {
            case CEE_NOP: break;

            case CEE_LDC_I4_M1:
            case CEE_LDC_I4_0:
            case CEE_LDC_I4_1:
            case CEE_LDC_I4_2:
            case CEE_LDC_I4_3:
            case CEE_LDC_I4_4:
            case CEE_LDC_I4_5:
            case CEE_LDC_I4_6:
            case CEE_LDC_I4_7:
            case CEE_LDC_I4_8:
                operand_i32 = (int32_t)opcode - CEE_LDC_I4_0;
            case CEE_LDC_I4_S:
            case CEE_LDC_I4: {
                MIR_op_t op;
                CHECK_AND_RETHROW(stack_push(&ctx, tSystem_Int32, &op));
//                MIR_append_insn(ctx.context, ctx.func,
//                                MIR_new_insn(ctx.context, MIR_MOV,
//                                             op,
//                                             MIR_new_int_op(ctx.context, operand_i32)));
            } break;

            default: {
                CHECK_FAIL("TODO: opcode %s", opcode_info->name);
            } break;
        }

    }

cleanup:
    MIR_finish_func(ctx.context);
    MIR_finish_module(ctx.context);

    MIR_output(ctx.context, stdout);

    MIR_finish(ctx.context);

    return NO_ERROR;
}

#pragma clang diagnostic pop