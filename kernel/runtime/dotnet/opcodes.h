#pragma once

#include "types.h"

typedef enum opcode_stack_behaviour {
    OPCODE_STACK_BEHAVIOUR_POP0,
    OPCODE_STACK_BEHAVIOUR_POP1,
    OPCODE_STACK_BEHAVIOUR_POP1_POP1,
    OPCODE_STACK_BEHAVIOUR_POPI,
    OPCODE_STACK_BEHAVIOUR_POPI_POP1,
    OPCODE_STACK_BEHAVIOUR_POPI_POPI,
    OPCODE_STACK_BEHAVIOUR_POPI_POPI_POPI,
    OPCODE_STACK_BEHAVIOUR_POPI8_POP8,
    OPCODE_STACK_BEHAVIOUR_POPI_POPR4,
    OPCODE_STACK_BEHAVIOUR_POPI_POPR8,
    OPCODE_STACK_BEHAVIOUR_POPREF,
    OPCODE_STACK_BEHAVIOUR_POPREF_POPI,
    OPCODE_STACK_BEHAVIOUR_POPREF_POPI_POPI,
    OPCODE_STACK_BEHAVIOUR_POPREF_POPI_POPI8,
    OPCODE_STACK_BEHAVIOUR_POPREF_POPI_POPR4,
    OPCODE_STACK_BEHAVIOUR_POPREF_POPI_POPR8,
    OPCODE_STACK_BEHAVIOUR_VARPOP,
    OPCODE_STACK_BEHAVIOUR_PUSH0,
    OPCODE_STACK_BEHAVIOUR_PUSH1,
    OPCODE_STACK_BEHAVIOUR_PUSH1_PUSH1,
    OPCODE_STACK_BEHAVIOUR_PUSHI,
    OPCODE_STACK_BEHAVIOUR_PUSHI8,
    OPCODE_STACK_BEHAVIOUR_PUSHR4,
    OPCODE_STACK_BEHAVIOUR_PUSHR8,
    OPCODE_STACK_BEHAVIOUR_PUSHREF,
    OPCODE_STACK_BEHAVIOUR_VARPUSH,
} opcode_stack_behaviour_t;

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
