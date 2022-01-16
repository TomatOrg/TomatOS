#include "jitter.h"

#include "jitter_internal.h"
#include "cil_opcode.h"
#include "mir_helpers.h"
#include "runtime.h"

#include <dotnet/metadata/signature.h>
#include <dotnet/builtin/string.h>
#include <dotnet/parameter_info.h>
#include <dotnet/method_info.h>
#include <dotnet/field_info.h>
#include <dotnet/assembly.h>
#include <dotnet/types.h>
#include <dotnet/gc/gc.h>
#include <dotnet/type.h>

#include <util/stb_ds.h>
#include <mir/mir.h>

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

static MIR_reg_t jit_push_temp(jitter_context_t* ctx) {
    char temp_name[32] = {0 };
    snprintf(temp_name, sizeof(temp_name), "ti%d", ctx->stack.temp);

    MIR_reg_t reg;
    if (ctx->stack.temp == ctx->stack.max_temp) {
        // need new reg
        ctx->stack.max_temp++;
        reg = MIR_new_func_reg(ctx->ctx, ctx->func, MIR_T_I64, temp_name);
    } else {
        // can reuse reg
        reg = MIR_reg(ctx->ctx, temp_name, ctx->func);
    }
    return reg;
}

static void jit_pop_temp(jitter_context_t* ctx) {
    ctx->stack.temp--;
}

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
    MIR_var_t* vars;
    buffer_t** var_names;
} mir_func_info_t;

static void destroy_mir_func_info(mir_func_info_t* func) {
    DESTROY_BUFFER(func->name);
    for (int i = 0; i < arrlen(func->var_names); i++) {
        DESTROY_BUFFER(func->var_names[i]);
    }
    arrfree(func->var_names);
    arrfree(func->vars);
}

static err_t setup_mir_func_info(mir_func_info_t* func, method_info_t method_info) {
    err_t err = NO_ERROR;

    // get the name
    func->name = create_buffer();
    method_full_name(method_info, func->name);
    bputc('\0', func->name);

    // setup the parameters
    func->vars = NULL;
    func->var_names = NULL;
    for (int i = 0; i < method_info->parameters_count; i++) {
        parameter_info_t parameter_info = &method_info->parameters[i];
        type_t parameter_type = parameter_info->parameter_type;

        // setup the var
        MIR_var_t var = {
            .name = parameter_info->name,
        };

        if (parameter_type->is_value_type) {
            // value types
            if (parameter_type->is_primitive) {
                // primitive types
                var.type = get_param_mir_type(parameter_type);
            } else {
                // value types
                var.type = MIR_T_BLK;
                var.size = parameter_type->stack_size;
            }
        } else {
            // reference objects
            var.type = MIR_T_P;
        }

        // create the name if needed
        if (var.name == NULL) {
            buffer_t* buffer = create_buffer();
            bprintf(buffer, "arg%d", i);
            bputc('\0', buffer);
            var.name = buffer->buffer;
            arrpush(func->var_names, buffer);
        }

        arrpush(func->vars, var);
    }

    // setup the return value
    func->ret_count = 0;
    func->ret_type = MIR_T_UNDEF;
    if (method_info->return_type != g_void) {
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

cleanup:
    if (IS_ERROR(err)) {
        destroy_mir_func_info(func);
    }
    return err;
}

static err_t jit_newobj(jitter_context_t* ctx, method_info_t ctor, MIR_op_t* out_op) {
    err_t err = NO_ERROR;
    buffer_t* name = NULL;

    name = create_buffer();
    type_full_name(ctor->declaring_type, name);
    bprintf(name, "$Type");
    bputc('\0', name);
    MIR_item_t item = mir_get_import(ctx->ctx, name->buffer);
    CHECK(item != NULL);

    MIR_reg_t temp = jit_push_temp(ctx);

    // allocate space for the item
    MIR_append_insn(ctx->ctx, ctx->func->func_item,
                    MIR_new_call_insn(ctx->ctx, 4,
                                      MIR_new_ref_op(ctx->ctx, ctx->gc_new_proto),
                                      MIR_new_ref_op(ctx->ctx, ctx->gc_new),
                                      MIR_new_reg_op(ctx->ctx, temp),
                                      MIR_new_ref_op(ctx->ctx, item)));
    // setup the arguments
    MIR_op_t* ops = NULL;

    arrpush(ops, MIR_new_ref_op(ctx->ctx, ctor->jit.proto));
    arrpush(ops, MIR_new_ref_op(ctx->ctx, ctor->jit.forward));

    // the first operand is the object we just allocated
    arrpush(ops, MIR_new_reg_op(ctx->ctx, temp));
    for (int i = 0; i < ctor->parameters_count - 1; i++) {
        arrpush(ops, jit_pop(ctx));
    }

    // call the function
    MIR_append_insn(ctx->ctx, ctx->func->func_item,
                    MIR_new_insn_arr(ctx->ctx, MIR_CALL, arrlen(ops), ops));

    // pop the top frame by setting the top of the stack to our
    // own stack frame
    MIR_append_insn(ctx->ctx, ctx->func->func_item,
                    MIR_new_call_insn(ctx->ctx, 3,
                                      MIR_new_ref_op(ctx->ctx, ctx->set_top_frame_proto),
                                      MIR_new_ref_op(ctx->ctx, ctx->set_top_frame),
                                      MIR_new_reg_op(ctx->ctx, ctx->stack.frame)));

cleanup:
    DESTROY_BUFFER(name);
    return err;
}

static err_t jit_throw(jitter_context_t* ctx, type_t exception_type) {
    err_t err = NO_ERROR;

    // find the correct ctor
    method_info_t ctor = NULL;
    for (int i = 0; i < exception_type->methods_count; i++) {
        method_info_t method_info = &exception_type->methods[i];
        if (strcmp(method_info->name, ".ctor") == 0) {
            if (method_info->parameters_count == 1) {
                ctor = method_info;
                break;
            }
        }
    }
    CHECK_ERROR(ctor != NULL, ERROR_NOT_FOUND);

    MIR_op_t op;
    CHECK_AND_RETHROW(jit_newobj(ctx, ctor, &op));

    // call the actual throw function
    MIR_append_insn(ctx->ctx, ctx->func->func_item,
                    MIR_new_call_insn(ctx->ctx, 3,
                                      MIR_new_ref_op(ctx->ctx, ctx->throw_proto),
                                      MIR_new_ref_op(ctx->ctx, ctx->throw),
                                      op));

cleanup:
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
    ctx->func = MIR_new_func_arr(ctx->ctx, func_info.name->buffer,
                             func_info.ret_count, &func_info.ret_type,
                             arrlen(func_info.vars), func_info.vars)->u.func;

    TRACE("%s", func_info.name->buffer);

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
        MIR_insn_code_t insn;
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

                // setup the arguments
                MIR_op_t* ops = NULL;

                arrpush(ops, MIR_new_ref_op(ctx->ctx, called_method_info->jit.proto));
                arrpush(ops, MIR_new_ref_op(ctx->ctx, called_method_info->jit.forward));

                for (int i = 0; i < called_method_info->parameters_count; i++) {
                    arrpush(ops, jit_pop(ctx));
                }

                if (called_method_info->return_type != g_void) {
                    arrins(ops, 2, jit_push(ctx, get_intermediate_type(called_method_info->return_type)));
                }

                // call the function
                MIR_append_insn(ctx->ctx, ctx->func->func_item,
                                MIR_new_insn_arr(ctx->ctx, MIR_CALL, arrlen(ops), ops));

                // pop the top frame by setting the top of the stack to our
                // own stack frame
                MIR_append_insn(ctx->ctx, ctx->func->func_item,
                                MIR_new_call_insn(ctx->ctx, 3,
                                                  MIR_new_ref_op(ctx->ctx, ctx->set_top_frame_proto),
                                                  MIR_new_ref_op(ctx->ctx, ctx->set_top_frame),
                                                  MIR_new_reg_op(ctx->ctx, ctx->stack.frame)));
            } break;

            case CIL_OPCODE_CONV_I1: insn = MIR_EXT8; goto cil_opcode_conv;
            case CIL_OPCODE_CONV_I2: insn = MIR_EXT16; goto cil_opcode_conv;
            case CIL_OPCODE_CONV_I4: insn = MIR_EXT32; goto cil_opcode_conv;
            case CIL_OPCODE_CONV_U1: insn = MIR_UEXT8; goto cil_opcode_conv;
            case CIL_OPCODE_CONV_U2: insn = MIR_UEXT16; goto cil_opcode_conv;
            case CIL_OPCODE_CONV_U4: insn = MIR_UEXT32; goto cil_opcode_conv;
            case CIL_OPCODE_CONV_I8:
            case CIL_OPCODE_CONV_U8:
            case CIL_OPCODE_CONV_I:
            case CIL_OPCODE_CONV_U:
                break; // nothing to do
            cil_opcode_conv: {
                type_t type = arrlast(ctx->stack.stack).type;
                CHECK (type == g_int || type == g_nint || type == g_long);
                MIR_op_t src = jit_pop(ctx);
                MIR_op_t dst = jit_push(ctx, g_int);
                MIR_append_insn(ctx->ctx, ctx->func->func_item,
                                MIR_new_insn(ctx->ctx, MIR_EXT8, dst, src));
            } break;

            // TODO: CIL_OPCODE_CONV_R4
            // TODO: CIL_OPCODE_CONV_R8
            // TODO: CIL_OPCODE_CONV_R_UN

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
                CHECK(i4 < method_info->parameters_count);
                parameter_info_t parameter_info = &method_info->parameters[i4];
                MIR_op_t dst = jit_push(ctx, get_intermediate_type(parameter_info->parameter_type));

                if (parameter_info->parameter_type->is_primitive || !parameter_info->parameter_type->is_value_type) {
                    // load by a simple move
                    MIR_append_insn(ctx->ctx, ctx->func->func_item,
                                    MIR_new_insn(ctx->ctx, MIR_MOV,
                                                 dst,
                                                 MIR_new_reg_op(ctx->ctx, MIR_reg(ctx->ctx, func_info.vars[i4].name, ctx->func))));
                } else {
                    CHECK_FAIL("TODO: value type arguments");
                }
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

                if (method_info->return_type != g_void) {
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

            case CIL_OPCODE_LDFLD: {
                token_t token = { .packed = FETCH_U4() };
                field_info_t field_info = assembly_get_field_info_by_token(method_info->assembly, token);
                CHECK_ERROR(field_info != NULL, ERROR_NOT_FOUND);

                // for debug
                temp_buffer = create_buffer();
                type_full_name(field_info->declaring_type, temp_buffer);
                printf(" %.*s.%s", arrlen(temp_buffer->buffer), temp_buffer->buffer, field_info->name);
                DESTROY_BUFFER(temp_buffer);

                // pop the value, but check that it is compatible before doing so
                CHECK(is_type_compatible_with(arrlast(ctx->stack.stack).type, field_info->declaring_type));
                MIR_op_t pos = jit_pop(ctx);

                // get the source operand
                MIR_op_t src;
                if (field_is_static(field_info)) {
                    CHECK_FAIL("TODO: Static variable");
                } else {
                    // we need a temp register to hold the base, since we need to first read
                    // it from the pointer stack
                    MIR_reg_t base = jit_push_temp(ctx);
                    MIR_append_insn(ctx->ctx, ctx->func->func_item,
                                    MIR_new_insn(ctx->ctx, MIR_MOV,
                                                 MIR_new_reg_op(ctx->ctx, base),
                                                 pos));

                    src = MIR_new_mem_op(ctx->ctx, get_param_mir_type(field_info->field_type),
                                   field_info->offset, base, 0, 0);
                }

                // push the intermediate type
                MIR_op_t dst = jit_push(ctx, get_intermediate_type(field_info->field_type));

                // mov from the object to the stack, figure the correct instruction
                // to use for this
                insn = MIR_MOV;
                if (field_info->field_type->is_primitive) {
                    if (field_info->field_type == g_ushort || field_info->field_type == g_char) {
                        insn = MIR_UEXT8;
                    } else if (field_info->field_type == g_byte || field_info->field_type == g_bool) {
                        insn = MIR_UEXT16;
                    } else if (field_info->field_type == g_sbyte) {
                        insn = MIR_EXT8;
                    } else if (field_info->field_type == g_short) {
                        insn = MIR_EXT16;
                    } else if (field_info->field_type == g_float) {
                        insn = MIR_F2D;
                    } else if (field_info->field_type == g_double) {
                        insn = MIR_DMOV;
                    }
                } else if (field_info->field_type->is_value_type) {
                    CHECK_FAIL("TODO: value types");
                }

                MIR_append_insn(ctx->ctx, ctx->func->func_item,
                                MIR_new_insn(ctx->ctx, insn, dst, src));

                if (!field_is_static(field_info)) {
                    // we no longer need this temp register
                    jit_pop_temp(ctx);
                }
            } break;

            case CIL_OPCODE_LDSTR: {
                int index = FETCH_U4() & 0x00ffffff;
                CHECK(index < method_info->assembly->us_size);

                // get the string itself
                size_t size = 0;
                const wchar_t* c = sig_parse_user_string(method_info->assembly->us + index, &size);
                printf(" \"%.*S\"", size / 2, c);

                // setup the string object
                MIR_op_t dst = jit_push(ctx, g_string);

                // Create the global instance
                char name[64] = {0};
                snprintf(name, sizeof(name), "str$%d", index);
                MIR_item_t item = mir_get_data(ctx->ctx, name);
                if (item == NULL) {
                    // create a new global string item
                    void* temp_data = malloc(sizeof(gc_header_t) + sizeof(system_string_t) + size);

                    // setup gc header
                    gc_header_t* header = temp_data;
                    header->type = g_string;

                    // setup the string itself
                    system_string_t* str = temp_data + sizeof(gc_header_t);
                    str->length = (int)size / 2;
                    memcpy(str->data, c, size);

                    // create the item itself
                    snprintf(name, sizeof(name), "str#%d", index);
                    item = MIR_new_data(ctx->ctx, name, MIR_T_U8, sizeof(gc_header_t) + sizeof(system_string_t) + size, temp_data);

                    // Create the referenced item
                    snprintf(name, sizeof(name), "str$%d", index);
                    item = MIR_new_ref_data(ctx->ctx, name, item, sizeof(gc_header_t));

                    // free the temp buffer
                    free(temp_data);
                }

                // Initialize the string nicely
                MIR_append_insn(ctx->ctx, ctx->func->func_item,
                                MIR_new_insn(ctx->ctx, MIR_MOV,
                                             dst,
                                             MIR_new_ref_op(ctx->ctx, item)));
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

static err_t create_type_import(jitter_context_t* ctx, type_t type) {
    err_t err = NO_ERROR;
    buffer_t* name = NULL;

    name = create_buffer();
    type_full_name(type, name);
    bprintf(name, "$Type");
    bputc('\0', name);

    MIR_new_import(ctx->ctx, name->buffer);

cleanup:
    DESTROY_BUFFER(name);

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
    method_info->jit.proto = MIR_new_proto_arr(ctx->ctx, func_info.name->buffer,
                                func_info.ret_count, &func_info.ret_type,
                               arrlen(func_info.vars), func_info.vars);

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
        jitter.set_top_frame_proto = MIR_new_proto(jitter.ctx, "$set_top_frame", 0, NULL, 1, MIR_T_P, "frame");
        jitter.set_top_frame = MIR_new_import(jitter.ctx, "set_top_frame");

        jitter.throw_proto = MIR_new_proto(jitter.ctx, "$throw", 0, NULL, 1, MIR_T_P, "exception");
        jitter.throw = MIR_new_import(jitter.ctx, "throw");

        MIR_type_t restype = MIR_T_P;
        jitter.gc_new_proto = MIR_new_proto(jitter.ctx, "gc_new_proto", 1, &restype, 1, MIR_T_P, "type");
        jitter.gc_new = MIR_new_import(jitter.ctx, "gc_new");
    }

    // TODO: import methods

    // forward declare all the methods we have in here
    // along side their prototypes
    for (int i = 0; i < assembly->types_count; i++) {
        type_t type = &assembly->types[i];

        create_type_import(&jitter, type);

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

        buffer_t* buffer = create_buffer();
        MIR_output(jitter.ctx, buffer);
        printf("%.*s", arrlen(buffer->buffer), buffer->buffer);
        destroy_buffer(buffer);

        MIR_finish(jitter.ctx);
    }
    return err;
}
