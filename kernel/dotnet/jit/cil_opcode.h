#pragma once

typedef enum cil_opcode {
    CIL_OPCODE_NOP = 0x00,
    CIL_OPCODE_BREAK = 0x01,
    CIL_OPCODE_LDARG_0 = 0x02,
    CIL_OPCODE_LDARG_1 = 0x03,
    CIL_OPCODE_LDARG_2 = 0x04,
    CIL_OPCODE_LDARG_3 = 0x05,
    CIL_OPCODE_LDLOC_0 = 0x06,
    CIL_OPCODE_LDLOC_1 = 0x07,
    CIL_OPCODE_LDLOC_2 = 0x08,
    CIL_OPCODE_LDLOC_3 = 0x09,
    CIL_OPCODE_STLOC_0 = 0x0a,
    CIL_OPCODE_STLOC_1 = 0x0b,
    CIL_OPCODE_STLOC_2 = 0x0c,
    CIL_OPCODE_STLOC_3 = 0x0d,
    CIL_OPCODE_LDARG_S = 0x0e,
    CIL_OPCODE_LDARGA_S = 0x0f,
    CIL_OPCODE_STARG_S = 0x10,
    CIL_OPCODE_LDLOC_S = 0x11,
    CIL_OPCODE_LDLOCA_S = 0x12,
    CIL_OPCODE_STLOC_S = 0x13,
    CIL_OPCODE_LDNULL = 0x14,
    CIL_OPCODE_LDC_I4_M1 = 0x15,
    CIL_OPCODE_LDC_I4_0 = 0x16,
    CIL_OPCODE_LDC_I4_1 = 0x17,
    CIL_OPCODE_LDC_I4_2 = 0x18,
    CIL_OPCODE_LDC_I4_3 = 0x19,
    CIL_OPCODE_LDC_I4_4 = 0x1a,
    CIL_OPCODE_LDC_I4_5 = 0x1b,
    CIL_OPCODE_LDC_I4_6 = 0x1c,
    CIL_OPCODE_LDC_I4_7 = 0x1d,
    CIL_OPCODE_LDC_I4_8 = 0x1e,
    CIL_OPCODE_LDC_I4_S = 0x1f,
    CIL_OPCODE_LDC_I4 = 0x20,
    CIL_OPCODE_LDC_I8 = 0x21,
    CIL_OPCODE_LDC_R4 = 0x22,
    CIL_OPCODE_LDC_R8 = 0x23,
    CIL_OPCODE_DUP = 0x25,
    CIL_OPCODE_POP = 0x26,
    CIL_OPCODE_JMP = 0x27,
    CIL_OPCODE_CALL = 0x28,
    CIL_OPCODE_CALLI = 0x29,
    CIL_OPCODE_RET = 0x2a,
    CIL_OPCODE_BR_S = 0x2b,
    CIL_OPCODE_BRFALSE_S = 0x2c,
    CIL_OPCODE_BRTRUE_S = 0x2d,
    CIL_OPCODE_BEQ_S = 0x2e,
    CIL_OPCODE_BGE_S = 0x2f,
    CIL_OPCODE_BGT_S = 0x30,
    CIL_OPCODE_BLE_S = 0x31,
    CIL_OPCODE_BLT_S = 0x32,
    CIL_OPCODE_BNE_UN_S = 0x33,
    CIL_OPCODE_BGE_UN_S = 0x34,
    CIL_OPCODE_BGT_UN_S = 0x35,
    CIL_OPCODE_BLE_UN_S = 0x36,
    CIL_OPCODE_BLT_UN_S = 0x37,
    CIL_OPCODE_BR = 0x38,
    CIL_OPCODE_BRFALSE = 0x39,
    CIL_OPCODE_BRTRUE = 0x3a,
    CIL_OPCODE_BEQ = 0x3b,
    CIL_OPCODE_BGE = 0x3c,
    CIL_OPCODE_BGT = 0x3d,
    CIL_OPCODE_BLE = 0x3e,
    CIL_OPCODE_BLT = 0x3f,
    CIL_OPCODE_BNE_UN = 0x40,
    CIL_OPCODE_BGE_UN = 0x41,
    CIL_OPCODE_BGT_UN = 0x42,
    CIL_OPCODE_BLE_UN = 0x43,
    CIL_OPCODE_BLT_UN = 0x44,
    CIL_OPCODE_SWITCH = 0x45,
    CIL_OPCODE_LDIND_I1 = 0x46,
    CIL_OPCODE_LDIND_U1 = 0x47,
    CIL_OPCODE_LDIND_I2 = 0x48,
    CIL_OPCODE_LDIND_U2 = 0x49,
    CIL_OPCODE_LDIND_I4 = 0x4a,
    CIL_OPCODE_LDIND_U4 = 0x4b,
    CIL_OPCODE_LDIND_I8 = 0x4c,
    CIL_OPCODE_LDIND_I = 0x4d,
    CIL_OPCODE_LDIND_R4 = 0x4e,
    CIL_OPCODE_LDIND_R8 = 0x4f,
    CIL_OPCODE_LDIND_REF = 0x50,
    CIL_OPCODE_STIND_REF = 0x51,
    CIL_OPCODE_STIND_I1 = 0x52,
    CIL_OPCODE_STIND_I2 = 0x53,
    CIL_OPCODE_STIND_I4 = 0x54,
    CIL_OPCODE_STIND_I8 = 0x55,
    CIL_OPCODE_STIND_R4 = 0x56,
    CIL_OPCODE_STIND_R8 = 0x57,
    CIL_OPCODE_ADD = 0x58,
    CIL_OPCODE_SUB = 0x59,
    CIL_OPCODE_MUL = 0x5a,
    CIL_OPCODE_DIV = 0x5b,
    CIL_OPCODE_DIV_UN = 0x5c,
    CIL_OPCODE_REM = 0x5d,
    CIL_OPCODE_REM_UN = 0x5e,
    CIL_OPCODE_AND = 0x5f,
    CIL_OPCODE_OR = 0x60,
    CIL_OPCODE_XOR = 0x61,
    CIL_OPCODE_SHL = 0x62,
    CIL_OPCODE_SHR = 0x63,
    CIL_OPCODE_SHR_UN = 0x64,
    CIL_OPCODE_NEG = 0x65,
    CIL_OPCODE_NOT = 0x66,
    CIL_OPCODE_CONV_I1 = 0x67,
    CIL_OPCODE_CONV_I2 = 0x68,
    CIL_OPCODE_CONV_I4 = 0x69,
    CIL_OPCODE_CONV_I8 = 0x6a,
    CIL_OPCODE_CONV_R4 = 0x6b,
    CIL_OPCODE_CONV_R8 = 0x6c,
    CIL_OPCODE_CONV_U4 = 0x6d,
    CIL_OPCODE_CONV_U8 = 0x6e,
    CIL_OPCODE_CALLVIRT = 0x6f,
    CIL_OPCODE_CPOBJ = 0x70,
    CIL_OPCODE_LDOBJ = 0x71,
    CIL_OPCODE_LDSTR = 0x72,
    CIL_OPCODE_NEWOBJ = 0x73,
    CIL_OPCODE_CASTCLASS = 0x74,
    CIL_OPCODE_ISINST = 0x75,
    CIL_OPCODE_CONV_R_UN = 0x76,
    CIL_OPCODE_UNBOX = 0x79,
    CIL_OPCODE_THROW = 0x7a,
    CIL_OPCODE_LDFLD = 0x7b,
    CIL_OPCODE_LDFLDA = 0x7c,
    CIL_OPCODE_STFLD = 0x7d,
    CIL_OPCODE_LDSFLD = 0x7e,
    CIL_OPCODE_LDSFLDA = 0x7f,
    CIL_OPCODE_STSFLD = 0x80,
    CIL_OPCODE_STOBJ = 0x81,
    CIL_OPCODE_CONV_OVF_I1_UN = 0x82,
    CIL_OPCODE_CONV_OVF_I2_UN = 0x83,
    CIL_OPCODE_CONV_OVF_I4_UN = 0x84,
    CIL_OPCODE_CONV_OVF_I8_UN = 0x85,
    CIL_OPCODE_CONV_OVF_U1_UN = 0x86,
    CIL_OPCODE_CONV_OVF_U2_UN = 0x87,
    CIL_OPCODE_CONV_OVF_U4_UN = 0x88,
    CIL_OPCODE_CONV_OVF_U8_UN = 0x89,
    CIL_OPCODE_CONV_OVF_I_UN = 0x8a,
    CIL_OPCODE_CONV_OVF_U_UN = 0x8b,
    CIL_OPCODE_BOX = 0x8c,
    CIL_OPCODE_NEWARR = 0x8d,
    CIL_OPCODE_LDLEN = 0x8e,
    CIL_OPCODE_LDELEMA = 0x8f,
    CIL_OPCODE_LDELEM_I1 = 0x90,
    CIL_OPCODE_LDELEM_U1 = 0x91,
    CIL_OPCODE_LDELEM_I2 = 0x92,
    CIL_OPCODE_LDELEM_U2 = 0x93,
    CIL_OPCODE_LDELEM_I4 = 0x94,
    CIL_OPCODE_LDELEM_U4 = 0x95,
    CIL_OPCODE_LDELEM_I8 = 0x96,
    CIL_OPCODE_LDELEM_I = 0x97,
    CIL_OPCODE_LDELEM_R4 = 0x98,
    CIL_OPCODE_LDELEM_R8 = 0x99,
    CIL_OPCODE_LDELEM_REF = 0x9a,
    CIL_OPCODE_STELEM_I = 0x9b,
    CIL_OPCODE_STELEM_I1 = 0x9c,
    CIL_OPCODE_STELEM_I2 = 0x9d,
    CIL_OPCODE_STELEM_I4 = 0x9e,
    CIL_OPCODE_STELEM_I8 = 0x9f,
    CIL_OPCODE_STELEM_R4 = 0xa0,
    CIL_OPCODE_STELEM_R8 = 0xa1,
    CIL_OPCODE_STELEM_REF = 0xa2,
    CIL_OPCODE_LDELEM = 0xa3,
    CIL_OPCODE_STELEM = 0xa4,
    CIL_OPCODE_UNBOX_ANY = 0xa5,
    CIL_OPCODE_CONV_OVF_I1 = 0xb3,
    CIL_OPCODE_CONV_OVF_U1 = 0xb4,
    CIL_OPCODE_CONV_OVF_I2 = 0xb5,
    CIL_OPCODE_CONV_OVF_U2 = 0xb6,
    CIL_OPCODE_CONV_OVF_I4 = 0xb7,
    CIL_OPCODE_CONV_OVF_U4 = 0xb8,
    CIL_OPCODE_CONV_OVF_I8 = 0xb9,
    CIL_OPCODE_CONV_OVF_U8 = 0xba,
    CIL_OPCODE_REFANYVAL = 0xc2,
    CIL_OPCODE_CKFINITE = 0xc3,
    CIL_OPCODE_MKREFANY = 0xc6,
    CIL_OPCODE_LDTOKEN = 0xd0,
    CIL_OPCODE_CONV_U2 = 0xd1,
    CIL_OPCODE_CONV_U1 = 0xd2,
    CIL_OPCODE_CONV_I = 0xd3,
    CIL_OPCODE_CONV_OVF_I = 0xd4,
    CIL_OPCODE_CONV_OVF_U = 0xd5,
    CIL_OPCODE_ADD_OVF = 0xd6,
    CIL_OPCODE_ADD_OVF_UN = 0xd7,
    CIL_OPCODE_MUL_OVF = 0xd8,
    CIL_OPCODE_MUL_OVF_UN = 0xd9,
    CIL_OPCODE_SUB_OVF = 0xda,
    CIL_OPCODE_SUB_OVF_UN = 0xdb,
    CIL_OPCODE_ENDFINALLY = 0xdc,
    CIL_OPCODE_LEAVE = 0xdd,
    CIL_OPCODE_LEAVE_S = 0xde,
    CIL_OPCODE_STIND_I = 0xdf,
    CIL_OPCODE_CONV_U = 0xe0,
    CIL_OPCODE_PREFIX7 = 0xf8,
    CIL_OPCODE_PREFIX6 = 0xf9,
    CIL_OPCODE_PREFIX5 = 0xfa,
    CIL_OPCODE_PREFIX4 = 0xfb,
    CIL_OPCODE_PREFIX3 = 0xfc,
    CIL_OPCODE_PREFIX2 = 0xfd,
    CIL_OPCODE_PREFIX1 = 0xfe,
    CIL_OPCODE_PREFIXREF = 0xff,
    CIL_OPCODE_ARGLIST = 0xfe00,
    CIL_OPCODE_CEQ = 0xfe01,
    CIL_OPCODE_CGT = 0xfe02,
    CIL_OPCODE_CGT_UN = 0xfe03,
    CIL_OPCODE_CLT = 0xfe04,
    CIL_OPCODE_CLT_UN = 0xfe05,
    CIL_OPCODE_LDFTN = 0xfe06,
    CIL_OPCODE_LDVIRTFTN = 0xfe07,
    CIL_OPCODE_LDARG = 0xfe09,
    CIL_OPCODE_LDARGA = 0xfe0a,
    CIL_OPCODE_STARG = 0xfe0b,
    CIL_OPCODE_LDLOC = 0xfe0c,
    CIL_OPCODE_LDLOCA = 0xfe0d,
    CIL_OPCODE_STLOC = 0xfe0e,
    CIL_OPCODE_LOCALLOC = 0xfe0f,
    CIL_OPCODE_ENDFILTER = 0xfe11,
    CIL_OPCODE_UNALIGNED_ = 0xfe12,
    CIL_OPCODE_VOLATILE_ = 0xfe13,
    CIL_OPCODE_TAIL_ = 0xfe14,
    CIL_OPCODE_INITOBJ = 0xfe15,
    CIL_OPCODE_CONSTRAINED_ = 0xfe16,
    CIL_OPCODE_CPBLK = 0xfe17,
    CIL_OPCODE_INITBLK = 0xfe18,
    CIL_OPCODE_RETHROW = 0xfe1a,
    CIL_OPCODE_SIZEOF = 0xfe1c,
    CIL_OPCODE_REFANYTYPE = 0xfe1d,
    CIL_OPCODE_READONLY = 0xfe1e,
} cil_opcode_t;

/**
 * Get the mnemonic of the given opcode
 *
 * @param opcode    [IN] Opcode
 */
const char* cil_opcode_to_str(cil_opcode_t opcode);