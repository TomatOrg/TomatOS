#pragma once

#include "types.h"

typedef enum opcode_stack_behaviour_pop {
    OPCODE_STACK_BEHAVIOUR_Pop0,
    OPCODE_STACK_BEHAVIOUR_Pop1,
    OPCODE_STACK_BEHAVIOUR_Pop1_Pop1,
    OPCODE_STACK_BEHAVIOUR_PopI,
    OPCODE_STACK_BEHAVIOUR_PopI_Pop1,
    OPCODE_STACK_BEHAVIOUR_PopI_PopI,
    OPCODE_STACK_BEHAVIOUR_PopI_PopI8,
    OPCODE_STACK_BEHAVIOUR_PopI_PopI_PopI,
    OPCODE_STACK_BEHAVIOUR_PopI8_Pop8,
    OPCODE_STACK_BEHAVIOUR_PopI_PopR4,
    OPCODE_STACK_BEHAVIOUR_PopI_PopR8,
    OPCODE_STACK_BEHAVIOUR_PopRef,
    OPCODE_STACK_BEHAVIOUR_PopRef_Pop1,
    OPCODE_STACK_BEHAVIOUR_PopRef_PopI,
    OPCODE_STACK_BEHAVIOUR_PopRef_PopI_Pop1,
    OPCODE_STACK_BEHAVIOUR_PopRef_PopI_PopI,
    OPCODE_STACK_BEHAVIOUR_PopRef_PopI_PopI8,
    OPCODE_STACK_BEHAVIOUR_PopRef_PopI_PopR4,
    OPCODE_STACK_BEHAVIOUR_PopRef_PopI_PopR8,
    OPCODE_STACK_BEHAVIOUR_PopRef_PopI_PopRef,
    OPCODE_STACK_BEHAVIOUR_VarPop,
} opcode_stack_behaviour_pop_t;

typedef enum opcode_stack_behaviour_push {
    OPCODE_STACK_BEHAVIOUR_Push0,
    OPCODE_STACK_BEHAVIOUR_Push1,
    OPCODE_STACK_BEHAVIOUR_Push1_Push1,
    OPCODE_STACK_BEHAVIOUR_PushI,
    OPCODE_STACK_BEHAVIOUR_PushI8,
    OPCODE_STACK_BEHAVIOUR_PushR4,
    OPCODE_STACK_BEHAVIOUR_PushR8,
    OPCODE_STACK_BEHAVIOUR_PushRef,
    OPCODE_STACK_BEHAVIOUR_VarPush,
} opcode_stack_behaviour_push_t;

typedef enum opcode_operand {
    OPCODE_OPERAND_InlineBrTarget,
    OPCODE_OPERAND_InlineField,
    OPCODE_OPERAND_InlineI,
    OPCODE_OPERAND_InlineI8,
    OPCODE_OPERAND_InlineMethod,
    OPCODE_OPERAND_InlineNone,
    OPCODE_OPERAND_InlineR,
    OPCODE_OPERAND_InlineSig,
    OPCODE_OPERAND_InlineString,
    OPCODE_OPERAND_InlineSwitch,
    OPCODE_OPERAND_InlineTok,
    OPCODE_OPERAND_InlineType,
    OPCODE_OPERAND_InlineVar,
    OPCODE_OPERAND_ShortInlineBrTarget,
    OPCODE_OPERAND_ShortInlineI,
    OPCODE_OPERAND_ShortInlineR,
    OPCODE_OPERAND_ShortInlineVar,
} opcode_operand_t;

typedef enum opcode_control_flow {
    OPCODE_CONTROL_FLOW_INVALID,
    OPCODE_CONTROL_FLOW_BRANCH,
    OPCODE_CONTROL_FLOW_CALL,
    OPCODE_CONTROL_FLOW_COND_BRANCH,
    OPCODE_CONTROL_FLOW_META,
    OPCODE_CONTROL_FLOW_NEXT,
    OPCODE_CONTROL_FLOW_RETURN,
    OPCODE_CONTROL_FLOW_THROW,
    OPCODE_CONTROL_FLOW_BREAK,
} opcode_control_flow_t;

typedef struct opcode_info {
    const char* name;
    opcode_operand_t operand;
    opcode_control_flow_t control_flow;
    opcode_stack_behaviour_pop_t pop;
    opcode_stack_behaviour_push_t push;
} opcode_info_t;

extern opcode_info_t g_dotnet_opcodes[];
extern int g_dotnet_opcodes_count;

typedef enum opcode {
    CEE_INVALID,
#define OPDEF_REAL_OPCODES_ONLY
#define OPDEF(cname, sname, pop, push, operand, kind, len, b1, b2, flow) cname,
#define OPALIAS(cname, sname, alias) cname = alias,
#include "metadata/opcode.def"
#undef OPALIAS
#undef OPDEF
#undef OPDEF_REAL_OPCODES_ONLY
} opcode_t;

extern uint16_t g_dotnet_opcode_lookup[];

void opcode_disasm_method(System_Reflection_MethodInfo method);
