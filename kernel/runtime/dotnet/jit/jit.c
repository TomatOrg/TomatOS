#include "jit.h"
#include "runtime/dotnet/opcodes.h"

#pragma clang diagnostic push
#pragma ide diagnostic ignored "UnreachableCode"
err_t jit_method(System_Reflection_MethodInfo method) {
    err_t err = NO_ERROR;

    System_Reflection_MethodBody body = method->MethodBody;
    System_Reflection_Assembly assembly = method->Module->Assembly;

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

            default: {
                CHECK_FAIL("TODO: opcode %s", opcode_info->name);
            } break;
        }

    }

cleanup:
    return err;
}

#pragma clang diagnostic pop