#include "jitter.h"

#include "jitter_internal.h"
#include "cil_opcode.h"
#include "mir_helpers.h"
#include "runtime.h"

#include <dotnet/method_info.h>
#include <dotnet/assembly.h>
#include <dotnet/parameter_info.h>
#include <dotnet/type.h>

#include <mir/mir.h>
#include <util/stb_ds.h>
#include <dotnet/types.h>
#include <dotnet/metadata/signature.h>

#define FETCH(type) \
    ({ \
        CHECK(code_end - code >= sizeof(type)); \
        type __value = *((type*)code); \
        code += sizeof(type); \
        __value; \
    })
#define FETCH_I1() FETCH(int8_t)
#define FETCH_I2() FETCH(int16_t)
#define FETCH_I4() FETCH(int32_t)
#define FETCH_I8() FETCH(int64_t)

#define FETCH_U1() FETCH(uint8_t)
#define FETCH_U2() FETCH(uint16_t)
#define FETCH_U4() FETCH(uint32_t)
#define FETCH_UI8() FETCH(uint64_t)

static MIR_op_t jit_push(jitter_context_t* ctx, type_t type) {
    // TODO: queue type for jitting

    MIR_op_t op;
    if (type->is_primitive || type->is_pointer) {
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

static MIR_type_t get_param_mir_type(type_t type) {
    if (type == g_sbyte) {
        return MIR_T_I8;
    } else if (type == g_byte || type == g_bool) {
        return MIR_T_U8;
    } else if (type == g_short) {
        return MIR_T_I16;
    } else if (type == g_ushort || type == g_char) {
        return MIR_T_U16;
    } else if (type == g_int) {
        return MIR_T_I32;
    } else if (type == g_uint) {
        return MIR_T_U32;
    } else if (type == g_long) {
        return MIR_T_I64;
    } else if (type == g_ulong) {
        return MIR_T_U64;
    } else if (type == g_float) {
        return MIR_T_F;
    } else if (type == g_double) {
        return MIR_T_D;
    } else if (type == g_nuint) {
        return type->stack_size == 4 ? MIR_T_U32 : MIR_T_U64;
    } else if (type == g_nint) {
        return type->stack_size == 4 ? MIR_T_I32 : MIR_T_I64;
    } else if (type->is_pointer) {
        return MIR_T_P;
    } else {
        return MIR_T_UNDEF;
    }
}

typedef struct mir_func_info {
    size_t ret_count;
    MIR_type_t ret_type;
    buffer_t* name;
} mir_func_info_t;

static void destroy_mir_func_info(mir_func_info_t* func) {
    DESTROY_BUFFER(func->name);
}

static err_t setup_mir_func_info(mir_func_info_t* func, method_info_t method_info) {
    err_t err = NO_ERROR;

    // get the name
    func->name = create_buffer();
    method_full_name(method_info, func->name);
    bputc('\0', func->name);

    // setup the return value
    func->ret_count = 0;
    func->ret_type = MIR_T_UNDEF;
    if (method_info->return_type != NULL) {
        func->ret_count = 1;
        if (method_info->return_type->is_value_type) {
            // value types
            if (method_info->return_type->is_primitive) {
                // primitive types
                func->ret_type = get_param_mir_type(method_info->return_type);
            } else {
                // value types (TODO)
                CHECK_FAIL("TODO: support value type returns");
            }
        } else {
            // reference objects
            func->ret_type = MIR_T_P;
        }
    }

    // TODO: setup arguments

cleanup:
    if (IS_ERROR(err)) {
        destroy_mir_func_info(func);
    }
    return err;
}

static err_t jitter_jit_method(jitter_context_t* ctx, method_info_t method_info) {
    err_t err = NO_ERROR;
    buffer_t* method_info_string = NULL;
    buffer_t* temp_buffer = NULL;
    mir_func_info_t func_info = {};

    // This will contain the method info itself
    method_info_string = create_buffer();
    method_full_name(method_info, method_info_string);
    bprintf(method_info_string, "$MethodInfo");
    bputc('\0', method_info_string);
    MIR_new_import(ctx->ctx, method_info_string->buffer);

    // TODO: setup method parameters
    CHECK_AND_RETHROW(setup_mir_func_info(&func_info, method_info));

    // create the function
    ctx->func = MIR_new_func(ctx->ctx, func_info.name->buffer,
                             func_info.ret_count, &func_info.ret_type,
                             0)->u.func;

    TRACE("%s", func_info.name->buffer);
    destroy_mir_func_info(&func_info);

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

        printf("[*] \t%s", cil_opcode_to_str(opcode));

        int32_t i4;
        switch (opcode) {
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // Base instructions
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////

            case CIL_OPCODE_CALL: {
                // get the method call
                token_t token = (token_t){ .packed = FETCH_U4() };
                method_info_t called_method_info = assembly_get_method_info_by_token(method_info->assembly, token);
                CHECK_ERROR(called_method_info != NULL, ERROR_NOT_FOUND);

                // for debug
                temp_buffer = create_buffer();
                method_full_name(called_method_info, temp_buffer);
                printf(" %.*s", arrlen(temp_buffer->buffer), temp_buffer->buffer);
                DESTROY_BUFFER(temp_buffer);

                // assume any call could throw an exception
                might_throw_exception = true;

                // TODO: arguments

                // pop the top frame by setting the top of the stack to our
                // own stack frame
                MIR_append_insn(ctx->ctx, ctx->func->func_item,
                                MIR_new_call_insn(ctx->ctx, 3,
                                                  MIR_new_ref_op(ctx->ctx, ctx->set_top_frame_proto),
                                                  MIR_new_ref_op(ctx->ctx, ctx->set_top_frame),
                                                  MIR_new_reg_op(ctx->ctx, ctx->stack.frame)));
            } break;

            case CIL_OPCODE_DUP: {
                CHECK(arrlen(ctx->stack.stack) >= 1);
                stack_item_t src = arrlast(ctx->stack.stack);
                MIR_op_t dst = jit_push(ctx, src.type);
                MIR_append_insn(ctx->ctx, ctx->func->func_item,
                                MIR_new_insn(ctx->ctx, MIR_MOV, dst, src));
            } break;

            case CIL_OPCODE_LDARG_0: i4 = 0; goto cil_opcode_ldarg;
            case CIL_OPCODE_LDARG_1: i4 = 1; goto cil_opcode_ldarg;
            case CIL_OPCODE_LDARG_2: i4 = 2; goto cil_opcode_ldarg;
            case CIL_OPCODE_LDARG_3: i4 = 3; goto cil_opcode_ldarg;
            case CIL_OPCODE_LDARG: i4 = FETCH_U2(); printf(" %d", i4); goto cil_opcode_ldarg;
            case CIL_OPCODE_LDARG_S: i4 = FETCH_U1(); printf(" %d", i4); goto cil_opcode_ldarg;
            cil_opcode_ldarg: {
                MIR_op_t dst = jit_push(ctx, method_info->parameters[i4].parameter_type);
                // TODO: load it
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
            case CIL_OPCODE_LDC_I4: i4 = FETCH_I4(); printf(" %d", i4); goto cil_opcode_ldc_i4;
            case CIL_OPCODE_LDC_I4_S: i4 = FETCH_I1(); printf(" %d", i4); goto cil_opcode_ldc_i4;
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
                // it is going to be the job of the caller to pop the stack
                // frame, this is to make sure the caller will properly store
                // the object reference returned (if any) before the stack frame is
                // popped and the reference from this frame is lost

                if (method_info->return_type != NULL) {
                    MIR_op_t ret = jit_pop(ctx);

                    // value types
                    if (method_info->return_type->is_primitive || !method_info->return_type->is_value_type) {
                        MIR_append_insn(ctx->ctx, ctx->func->func_item,
                                        MIR_new_ret_insn(ctx->ctx, 1, ret));
                    } else {
                        // value types (TODO)
                        CHECK_FAIL("TODO: support value type returns");
                    }
                } else {
                    MIR_append_insn(ctx->ctx, ctx->func->func_item,
                                    MIR_new_ret_insn(ctx->ctx, 0));
                }
            } break;

            ////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // Object model instructions
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////

            case CIL_OPCODE_LDSTR: {
                int index = FETCH_U4() & 0x00ffffff;
                CHECK(index < method_info->assembly->us_size);

                //
                size_t size = 0;
                const wchar_t* c = sig_parse_user_string(method_info->assembly->us + index, &size);
                printf(" \"%.*S\"\n", size / 2, c);

                MIR_op_t dst = jit_push(ctx, g_string);

                // TODO: initialize a new string properly
                MIR_append_insn(ctx->ctx, ctx->func->func_item,
                                MIR_new_insn(ctx->ctx, MIR_MOV, dst,
                                             MIR_new_int_op(ctx->ctx, 0)));
            } break;

            ////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // Default opcode
            ////////////////////////////////////////////////////////////////////////////////////////////////////////////

            default: {
                printf("\n");
                CHECK_FAIL("Invalid opcode!");
            } break;
        }

        printf("\n");
    } while (code < code_end);

    // add the stack frame only if it is needed, we need it whenever we could have exceptions
    // (so we can have it in the stack trace) or when we actually have stack items that the gc
    // might need to scan.
    if (might_throw_exception || ctx->stack.max_o > 0) {
        // Allocate the stack frame
        MIR_insn_t stack_frame_setup = MIR_new_insn(ctx->ctx, MIR_ALLOCA,
                                                    MIR_new_reg_op(ctx->ctx, ctx->stack.frame),
                                                    MIR_new_int_op(ctx->ctx, STACK_FRAME_OBJECTS_OFFSET + ctx->stack.max_o * 8));
        MIR_prepend_insn(ctx->ctx, ctx->func->func_item, stack_frame_setup);

        // Zero the prev for first init
        MIR_APPEND(stack_frame_setup, MIR_new_insn(ctx->ctx, MIR_MOV,
                                                   MIR_new_mem_op(ctx->ctx, MIR_T_I64,
                                                                  STACK_FRAME_PREV_OFFSET, ctx->stack.frame, 0, 0),
                                                   MIR_new_int_op(ctx->ctx, 0)));

        // Set the method
        MIR_APPEND(stack_frame_setup, MIR_new_insn(ctx->ctx, MIR_MOV,
                                                   MIR_new_mem_op(ctx->ctx, MIR_T_I64,
                                                                  STACK_FRAME_METHOD_INFO_OFFSET, ctx->stack.frame, 0, 0),
                                                   MIR_new_ref_op(ctx->ctx, mir_get_import(ctx->ctx, method_info_string->buffer))));

        // setup the count
        MIR_APPEND(stack_frame_setup, MIR_new_insn(ctx->ctx, MIR_MOV,
                                                   MIR_new_mem_op(ctx->ctx, MIR_T_I16,
                                                                  STACK_FRAME_OBJECT_COUNT_OFFSET, ctx->stack.frame, 0, 0),
                                                   MIR_new_int_op(ctx->ctx, ctx->stack.max_o)));

        // zero out the whole stack frame
        mir_emit_inline_memset(ctx, stack_frame_setup,
                               ctx->stack.frame, STACK_FRAME_OBJECTS_OFFSET,
                               0x00, ctx->stack.max_o * 8);

        // Link it to the rest of the stack
        MIR_APPEND(stack_frame_setup, MIR_new_call_insn(ctx->ctx, 3,
                                          MIR_new_ref_op(ctx->ctx, ctx->set_top_frame_proto),
                                          MIR_new_ref_op(ctx->ctx, ctx->set_top_frame),
                                          MIR_new_reg_op(ctx->ctx, ctx->stack.frame)));
    }

cleanup:
    MIR_finish_func(ctx->ctx);

    destroy_mir_func_info(&func_info);

    DESTROY_BUFFER(temp_buffer);
    DESTROY_BUFFER(method_info_string);

    arrfree(ctx->stack.stack);
    memset(&ctx->stack, 0, sizeof(ctx->stack));

    return err;
}

static err_t create_method_proto_and_forward(jitter_context_t* ctx, method_info_t method_info) {
    err_t err = NO_ERROR;
    mir_func_info_t func_info = {};

    // setup the info
    CHECK_AND_RETHROW(setup_mir_func_info(&func_info, method_info));

    // create the forward
    method_info->jit.forward = MIR_new_forward(ctx->ctx, func_info.name->buffer);

    TRACE("%s", func_info.name->buffer);

    // add the prototype suffix
    arrpop(func_info.name->buffer);
    bprintf(func_info.name, "$Prototype");
    bputc('\0', func_info.name);

    // create the function
    method_info->jit.proto = MIR_new_proto(ctx->ctx, func_info.name->buffer,
                                func_info.ret_count, &func_info.ret_type,
                                0);

cleanup:
    destroy_mir_func_info(&func_info);
    return err;
}

err_t jitter_jit_assembly(assembly_t assembly) {
    err_t err = NO_ERROR;

    // init the jitter
    jitter_context_t jitter = {
        .ctx = MIR_init()
    };
    CHECK_ERROR(jitter.ctx != NULL, ERROR_OUT_OF_RESOURCES);

    // setup the module name
    MIR_new_module(jitter.ctx, assembly->name);

    // import static stuff
    {
        // the set_top_frame
        {
            jitter.set_top_frame_proto = MIR_new_proto(jitter.ctx, "$set_top_frame", 0, NULL, 1, MIR_T_P, "frame");
            jitter.set_top_frame = MIR_new_import(jitter.ctx, "set_top_frame");
        }
    }

    // TODO: import methods

    // forward declare all the methods we have in here
    // along side their prototypes
    for (int i = 0; i < assembly->types_count; i++) {
        type_t type = &assembly->types[i];
        for (int j = 0; j < type->methods_count; j++) {
            method_info_t method_info = &type->methods[j];
            CHECK_AND_RETHROW(create_method_proto_and_forward(&jitter, method_info));
        }
    }

    // Transform all the types to MIR
    for (int i = 0; i < assembly->types_count; i++) {
        type_t type = &assembly->types[i];
        for (int j = 0; j < type->methods_count; j++) {
            method_info_t method_info = &type->methods[j];
            CHECK_AND_RETHROW(jitter_jit_method(&jitter, method_info));
        }
    }

cleanup:
    if (jitter.ctx != NULL) {
        MIR_finish_module(jitter.ctx);

//        buffer_t* buffer = create_buffer();
//        MIR_output(jitter.ctx, buffer);
//        printf("%.*s", arrlen(buffer->buffer), buffer->buffer);
//        destroy_buffer(buffer);

        MIR_finish(jitter.ctx);
    }
    return err;
}
