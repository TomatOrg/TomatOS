#include "signature.h"
#include "metadata_spec.h"

#include <dotnet/parameter_info.h>
#include <dotnet/method_info.h>
#include <dotnet/field_info.h>
#include <dotnet/assembly.h>
#include <dotnet/types.h>
#include <dotnet/type.h>

#include <mem/malloc.h>

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
    ELEMENT_TYPE_BYREF = 0x10,
    ELEMENT_TYPE_VALUETYPE = 0x11,
    ELEMENT_TYPE_CLASS = 0x12,
    ELEMENT_TYPE_VAR = 0x13,
    ELEMENT_TYPE_I = 0x18,
    ELEMENT_TYPE_U = 0x19,
    ELEMENT_TYPE_OBJECT = 0x1c,
    ELEMENT_TYPE_SZARRAY = 0x1d,
    ELEMENT_TYPE_CMOD_REQD = 0x1f,
    ELEMENT_TYPE_CMOD_OPT = 0x20,
} element_type_t;

typedef struct sig {
    const uint8_t* entry;
} sig_t;

static int sig_get_entry(sig_t* sig) {
    unsigned char a,b,c,d;
    a = *sig->entry++;
    if ((a & 0x80) == 0) {
        // 1-byte entry
        return a;
    }

    // Special case
    if (a == 0xff) {
        return 0;
    }

    b = *sig->entry++;
    if ((a & 0xc0) == 0x80) {
        // 2-byte entry
        return ((int)(a & 0x3f)) << 8 | b;
    }

    // 4-byte entry
    c = *sig->entry++;
    d = *sig->entry++;
    return ((int)(a & 0x1f)) << 24 | ((int)b) << 16 | ((int)c) << 8 | d;
}

static uint8_t m_table_id[] = {
    METADATA_TYPE_DEF,
    METADATA_TYPE_REF,
    METADATA_TYPE_SPEC,
    0
};

static token_t sig_get_type_def_or_ref_or_spec(sig_t* sig) {
    uint32_t entry = sig_get_entry(sig);
    return (token_t){ .table = m_table_id[entry & 0x3], .index = entry >> 2 };
}

static err_t sig_get_type(assembly_t assembly, sig_t* sig, type_t* type) {
    err_t err = NO_ERROR;

    int entry = sig_get_entry(sig);
    switch (entry) {
        // short forms
        case ELEMENT_TYPE_VOID: *type = g_void; break;
        case ELEMENT_TYPE_BOOLEAN: *type = g_bool; break;
        case ELEMENT_TYPE_CHAR: *type = g_char; break;
        case ELEMENT_TYPE_I1: *type = g_sbyte; break;
        case ELEMENT_TYPE_U1: *type = g_byte; break;
        case ELEMENT_TYPE_I2: *type = g_short; break;
        case ELEMENT_TYPE_U2: *type = g_ushort; break;
        case ELEMENT_TYPE_I4: *type = g_int; break;
        case ELEMENT_TYPE_U4: *type = g_uint; break;
        case ELEMENT_TYPE_I8: *type = g_long; break;
        case ELEMENT_TYPE_U8: *type = g_ulong; break;
        case ELEMENT_TYPE_R4: *type = g_float; break;
        case ELEMENT_TYPE_R8: *type = g_double; break;
        case ELEMENT_TYPE_STRING: *type = g_string; break;
        case ELEMENT_TYPE_I: *type = g_nint; break;
        case ELEMENT_TYPE_U: *type = g_nuint; break;
        case ELEMENT_TYPE_OBJECT: *type = g_object; break;

        // class reference
        case ELEMENT_TYPE_VALUETYPE:
        case ELEMENT_TYPE_CLASS: {
            token_t token = sig_get_type_def_or_ref_or_spec(sig);
            *type = assembly_get_type_by_token(assembly, token);
            CHECK(*type != NULL);
        } break;

            // pointer type
        case ELEMENT_TYPE_PTR: {
            type_t element = NULL;
            CHECK_AND_RETHROW(sig_get_type(assembly, sig, &element));
            *type = make_pointer_type(element);
        } break;

            // unknown entry
        default: CHECK_FAIL("Invalid entry: %x", entry);
    }

    cleanup:
    return err;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static size_t sig_get_length(sig_t* sig) {
    return sig_get_entry(sig);
}

static err_t sig_parse_ret_type(sig_t* sig, assembly_t assembly, type_t* type) {
    err_t err = NO_ERROR;
    bool by_ref = false;

    sig_t temp = *sig;
    int entry = sig_get_entry(sig);
    switch (entry) {
        // TODO: TYPEBYREF

        // VOID
        case ELEMENT_TYPE_VOID:
            *type = g_void;
            break;

            // BYREF
        case ELEMENT_TYPE_BYREF:
            temp = *sig;
            by_ref = true;

            // fallthrough to Type

            // Type
        default: {
            *sig = temp;
            CHECK_AND_RETHROW(sig_get_type(assembly, sig, type));
            if (by_ref) {
                *type = make_by_ref_type(*type);
                CHECK(*type != NULL);
            }
        }

    }

    cleanup:
    return err;
}

static err_t sig_parse_param(sig_t* sig, assembly_t assembly, type_t* type) {
    err_t err = NO_ERROR;
    bool by_ref = false;

    sig_t temp = *sig;
    int entry = sig_get_entry(sig);
    switch (entry) {
        // TODO: TYPEBYREF

        // BYREF
        case ELEMENT_TYPE_BYREF:
            temp = *sig;
            by_ref = true;
            // fallthrough to Type

            // Type
        default: {
            *sig = temp;
            CHECK_AND_RETHROW(sig_get_type(assembly, sig, type));
            if (by_ref) {
                *type = make_by_ref_type(*type);
                CHECK(*type != NULL);
            }
        }
    }

    cleanup:
    return err;
}

err_t sig_parse_field(const uint8_t* signature, assembly_t assembly, field_info_t field) {
    err_t err = NO_ERROR;
    sig_t sig = {
            .entry = signature
    };

    sig_get_length(&sig);

    // must start with field spec
    CHECK(sig_get_entry(&sig) == 0x06);

    // get the type
    CHECK_AND_RETHROW(sig_get_type(assembly, &sig, &field->field_type));

    cleanup:
    return err;
}

//err_t sig_parse_method_locals(sig_t* sig, method_t* method) {
//    err_t err = NO_ERROR;
//
//    sig_get_length(sig);
//
//    // LOCAL_SIG
//    CHECK(sig_get_entry(sig) == 0x07);
//
//    // Count
//    method->locals_count = sig_get_entry(sig);
//    method->locals = calloc(method->locals_count, sizeof(local_t));
//
//    for (int i = 0; i < method->locals_count; i++) {
//        CHECK_AND_RETHROW(sig_parse_param(sig, method->assembly, &method->locals[i].type));
//    }
//
//cleanup:
//    return err;
//}

err_t sig_parse_method(const uint8_t* signature, method_info_t method) {
    err_t err = NO_ERROR;
    sig_t sig = {
            .entry = signature
    };

    sig_get_length(&sig);

    // starting from the first block
    int entry = *sig.entry++;

    // this stuff
    bool has_this = false;
    bool explicit_this = false;
    if (entry & 0x20) {
        // HASTHIS
        has_this = true;

        if (entry & 0x40) {
            // EXPLICITTHIS
            CHECK_FAIL("TODO: EXPLICITTHIS");
            explicit_this = true;

        } else {
            method->parameters_count = 1;
        }
    }

    // calling convention
    switch (entry & 0x1f) {
        case 0x00: break; // DEFAULT
        case 0x05: CHECK_FAIL("TODO: VARARG"); break; // VARARG
        case 0x10: CHECK_FAIL("TODO: GENERIC"); break; // GENERIC
    }

    // ParamCount
    int offset = method->parameters_count;
    method->parameters_count += sig_get_entry(&sig);
    if (method->parameters_count > 0) {
        method->parameters = malloc(method->parameters_count * sizeof(struct parameter_info));
        CHECK_ERROR(method->parameters != NULL, ERROR_OUT_OF_RESOURCES);
    } else {
        method->parameters = NULL;
    }

    // RetType
    CHECK_AND_RETHROW(sig_parse_ret_type(&sig, method->assembly, &method->return_type));

    // Params
    for (int i = offset; i < method->parameters_count; i++) {
        CHECK_AND_RETHROW(sig_parse_param(&sig, method->assembly, &method->parameters[i].parameter_type));
        method->parameters[i].assembly = method->assembly;
        method->parameters[i].declaring_method = method;
        method->parameters[i].position = i;
    }

    if (has_this) {
        type_t this_type = method->declaring_type;
        if (this_type->is_value_type) {
            this_type = make_by_ref_type(this_type);
        }

        method->parameters[0].parameter_type = this_type;
        method->parameters[0].assembly = method->assembly;
        method->parameters[0].declaring_method = method;
        method->parameters[0].position = 0;
        method->parameters[0].name = "this";
    }

    cleanup:
    return err;
}
