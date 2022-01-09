#include "jitter.h"

#include "jitter_internal.h"
#include "cil_opcode.h"

#include <dotnet/method_info.h>
#include <dotnet/type.h>

#include <mir/mir.h>

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
        type __value = *(type*)code; \
        code += sizeof(type); \
        __value; \
    })
#define FETCH_I4() FETCH(int32_t)
#define FETCH_U4() FETCH(uint32_t)
#define FETCH_I2() FETCH(int16_t)
#define FETCH_U2() FETCH(uint16_t)

static MIR_op_t jit_pop() {

}

static void jit_push(type_t type, MIR_op_t op) {

}

err_t jitter_jit_method(jitter_context_t* ctx, method_info_t method_info) {
    err_t err = NO_ERROR;

    // TODO: properly
    MIR_new_module(ctx->ctx, method_info->declaring_type->name);
    MIR_new_func(ctx->ctx, method_info->name, 0, NULL, 0);
    TRACE("%s.%s.%s", method_info->declaring_type->namespace, method_info->declaring_type->name, method_info->name);

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

        int32_t i32;
        switch (opcode) {
            case CIL_OPCODE_ADD_OVF: {

            } break;
        }
    } while (code < code_end);

cleanup:
    MIR_finish_func(ctx->ctx);
    MIR_finish_module(ctx->ctx);
    return err;
}

void destroy_jitter(jitter_context_t* jitter) {
    if (jitter == NULL) return;

    if (jitter->ctx != NULL) {
        MIR_finish(jitter->ctx);
    }
    free(jitter);
}

