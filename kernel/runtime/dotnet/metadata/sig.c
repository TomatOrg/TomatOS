#include "sig.h"

#include "sig_spec.h"

#define NEXT_BYTE \
    do { \
        sig->data++; \
        sig->size--; \
    } while (0)

#define CONSUME_BYTE() \
    ({ \
        CHECK(sig->size > 0); \
        uint8_t b = sig->data[0]; \
        NEXT_BYTE; \
        b; \
    })

#define EXPECT_BYTE(value) \
    do { \
        CHECK(sig->size > 0); \
        CHECK(sig->data[0] == (value), "Expected %d, but got %d", value, sig->data[0]); \
        NEXT_BYTE; \
    } while (0)


static err_t parse_custom_mod(blob_entry_t* sig, bool* found) {
    err_t err = NO_ERROR;

    CHECK(sig->size > 0);

    if (sig->data[0] == ELEMENT_TYPE_CMOD_OPT || sig->data[0] == ELEMENT_TYPE_CMOD_REQD) {
        // got a custom mod
        NEXT_BYTE;

        // TODO: this
        ASSERT(!"TODO: handle this!");

        *found = true;
    } else {
        *found = false;
    }

cleanup:
    return err;
}

static int m_idx_to_table[] = {
    [0] = METADATA_TYPE_DEF,
    [1] = METADATA_TYPE_REF,
    [2] = METADATA_TYPE_SPEC,
    [3] = -1
};

err_t parse_compressed_integer(blob_entry_t* sig, uint32_t* value) {
    err_t err = NO_ERROR;

    uint8_t a = CONSUME_BYTE();
    if ((a & 0x80) == 0) {
        *value = a;
        goto cleanup;
    }

    uint8_t b = CONSUME_BYTE();
    if ((a & 0xc0) == 0x80) {
        // 2-byte entry
        *value = ((int)(a & 0x3f)) << 8 | b;
        goto cleanup;
    }

    // 4-byte entry
    uint8_t c = CONSUME_BYTE();
    uint8_t d = CONSUME_BYTE();
    *value = ((int)(a & 0x1f)) << 24 | ((int)b) << 16 | ((int)c) << 8 | d;

cleanup:
    return err;
}

static err_t parse_type_def_or_ref_or_spec_encoded(blob_entry_t* sig, token_t* token) {
    err_t err = NO_ERROR;

    // get the compressed index
    uint32_t index;
    CHECK_AND_RETHROW(parse_compressed_integer(sig, &index));

    // get the table
    int table = m_idx_to_table[index & 0b11];
    CHECK(table != -1);

    // get the real index
    index >>= 2;

    // encode nicely
    *token = (token_t) {
        .table = table,
        .index = index
    };

cleanup:
    return err;
}

static err_t parse_type(System_Reflection_Assembly assembly, blob_entry_t* sig, System_Type* out_type, bool allow_void) {
    err_t err = NO_ERROR;

    // switchhh
    uint8_t element_type = CONSUME_BYTE();
    switch (element_type) {
        case ELEMENT_TYPE_VOID: CHECK(allow_void); *out_type = NULL; break;
        case ELEMENT_TYPE_BOOLEAN: *out_type = tSystem_Boolean; break;
        case ELEMENT_TYPE_CHAR: *out_type = tSystem_Char; break;
        case ELEMENT_TYPE_I1: *out_type = tSystem_SByte; break;
        case ELEMENT_TYPE_U1: *out_type = tSystem_Byte; break;
        case ELEMENT_TYPE_I2: *out_type = tSystem_Int16; break;
        case ELEMENT_TYPE_U2: *out_type = tSystem_UInt16; break;
        case ELEMENT_TYPE_I4: *out_type = tSystem_Int32; break;
        case ELEMENT_TYPE_U4: *out_type = tSystem_UInt32; break;
        case ELEMENT_TYPE_I8: *out_type = tSystem_Int64; break;
        case ELEMENT_TYPE_U8: *out_type = tSystem_UInt64; break;
        case ELEMENT_TYPE_R4: *out_type = tSystem_Single; break;
        case ELEMENT_TYPE_R8: *out_type = tSystem_Double; break;
        case ELEMENT_TYPE_I: *out_type = tSystem_IntPtr; break;
        case ELEMENT_TYPE_U: *out_type = tSystem_UIntPtr; break;
        // TODO: array

        case ELEMENT_TYPE_VALUETYPE:
        case ELEMENT_TYPE_CLASS: {
            token_t token;
            CHECK_AND_RETHROW(parse_type_def_or_ref_or_spec_encoded(sig, &token));
            *out_type = get_type_by_token(assembly, token);
            CHECK(*out_type != NULL);
        } break;

        // TODO: fnptr
        // TODO: genericinst
        // TODO: mvar
        case ELEMENT_TYPE_OBJECT: *out_type = tSystem_Object; break;

        case ELEMENT_TYPE_PTR: {
            *out_type = tSystem_UIntPtr;

            // TODO: pointer types that store their actual type?
            System_Type elementType;
            CHECK_AND_RETHROW(parse_type(assembly, sig, &elementType, true));
        } break;

        case ELEMENT_TYPE_STRING: *out_type = tSystem_String; break;

        case ELEMENT_TYPE_SZARRAY: {
            System_Type elementType = NULL;
            CHECK_AND_RETHROW(parse_type(assembly, sig, &elementType, false));
            *out_type = get_array_type(elementType);
        } break;

        // TODO: ELEMENT_TYPE_VALUETYPE: make sure the given type actually is value type

        // TODO: var

        default: CHECK_FAIL("Got invalid element type: 0x%02x", element_type);
    }

cleanup:
    return err;
}

err_t parse_field_sig(blob_entry_t _sig, System_Reflection_FieldInfo field) {
    err_t err = NO_ERROR;
    blob_entry_t* sig = &_sig;

    // make sure this even points to a field
    EXPECT_BYTE(FIELD);

    // get custom mods
    // TODO: wtf is a custom mod
    bool found = false;
    do {
        CHECK_AND_RETHROW(parse_custom_mod(sig, &found));
    } while (found);

    // parse the actual field
    CHECK_AND_RETHROW(parse_type(field->Module->Assembly, sig, &field->FieldType, false));

cleanup:
    return err;
}
