#pragma once

#define FIELD 0x6
#define LOCAL_SIG 0x7

#define HASTHIS 0x20
#define EXPLICITTHIS 0x40
#define DEFAULT 0x0
#define VARARG 0x5
#define C 0x1
#define STDCALL 0x2
#define THISCALL 0x3
#define FASTCALL 0x4
#define SENTINEL 0x41

typedef enum element_type {
    ELEMENT_TYPE_END = 0x00,
    ELEMENT_TYPE_VOID = 0x01,
    ELEMENT_TYPE_BOOLEAN = 0x02,
    ELEMENT_TYPE_CHAR = 0x03,
    ELEMENT_TYPE_I1 = 0x04,
    ELEMENT_TYPE_U1 = 0x05,
    ELEMENT_TYPE_I2 = 0x06,
    ELEMENT_TYPE_U2 = 0x07,
    ELEMENT_TYPE_I4 = 0x08,
    ELEMENT_TYPE_U4 = 0x09,
    ELEMENT_TYPE_I8 = 0x0a,
    ELEMENT_TYPE_U8 = 0x0b,
    ELEMENT_TYPE_R4 = 0x0c,
    ELEMENT_TYPE_R8 = 0x0d,
    ELEMENT_TYPE_STRING = 0x0e,
    ELEMENT_TYPE_PTR = 0x0f,
    ELEMENT_TYPE_BYREF = 0x0f,
    ELEMENT_TYPE_VALUETYPE = 0x11,
    ELEMENT_TYPE_CLASS = 0x12,
    ELEMENT_TYPE_VAR = 0x13,
    ELEMENT_TYPE_ARRAY = 0x14,
    ELEMENT_TYPE_GENERICINST = 0x15,
    ELEMENT_TYPE_TYPEDBYREF = 0x16,
    // 0x17
    ELEMENT_TYPE_I = 0x18,
    ELEMENT_TYPE_U = 0x19,
    // 0x1a
    ELEMENT_TYPE_FNPTR = 0x1b,
    ELEMENT_TYPE_OBJECT = 0x1c,
    ELEMENT_TYPE_SZARRAY = 0x1d,
    ELEMENT_TYPE_MVAR = 0x1e,
    ELEMENT_TYPE_CMOD_REQD = 0x1f,
    ELEMENT_TYPE_CMOD_OPT = 0x20,
    ELEMENT_TYPE_INTERNAL = 0x21,
    // ...
    ELEMENT_TYPE_MODIFIER = 0x40,
    ELEMENT_TYPE_SENTINEL = 0x41,
    ELEMENT_TYPE_PINNED = 0x45,
} element_type_t;
