#include "jit.h"

#include "runtime/dotnet/opcodes.h"
#include "util/stb_ds.h"
#include "mir/mir.h"
#include "time/timer.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Type helpers
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static MIR_type_t get_mir_type(System_Type type) {
    type = type_get_underlying_type(type);

    if (type == tSystem_Byte) {
        return MIR_T_U8;
    } else if (type == tSystem_SByte) {
        return MIR_T_I8;
    } else if (type == tSystem_UInt16) {
        return MIR_T_U16;
    } else if (type == tSystem_Int16) {
        return MIR_T_I16;
    } else if (type == tSystem_UInt32) {
        return MIR_T_U32;
    } else if (type == tSystem_Int32) {
        return MIR_T_I32;
    } else if (type == tSystem_UInt64) {
        return MIR_T_U64;
    } else if (type == tSystem_Int64) {
        return MIR_T_I64;
    } else if (type == tSystem_UIntPtr) {
        return MIR_T_U64;
    } else if (type == tSystem_IntPtr) {
        return MIR_T_I64;
    } else if (type == tSystem_Char) {
        return MIR_T_U16;
    } else if (type == tSystem_Boolean) {
        return MIR_T_I8;
    } else if (type == tSystem_Single) {
        return MIR_T_F;
    } else if (type == tSystem_Double) {
        return MIR_T_D;
    } else if (type->IsValueType) {
        return MIR_T_BLK;
    } else {
        return MIR_T_P;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The context of the jit
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct stack_entry {
    // the type of the stack entry
    System_Type type;

    // the location where this value
    // is stored on the stack
    MIR_reg_t reg;
} stack_entry_t;

typedef struct stack {
    // the stack entries
    stack_entry_t* entries;
} stack_t;

typedef struct function_entry {
    System_Reflection_MethodInfo key;
    MIR_item_t proto;
    MIR_item_t forward;
} function_entry_t;

typedef struct stack_snapshot {
    int key;
    stack_t stack;
    MIR_label_t label;
} stack_snapshot_t;

typedef struct jit_context {
    stack_snapshot_t* pc_to_stack_snapshot;

    // the index to the current stack
    stack_t stack;

    // the function that this stack is for
    MIR_item_t func;

    // the current method being compiled
    System_Reflection_MethodInfo method_info;

    // used for name generation
    int name_gen;

    //////////////////////////////////////////////////////////////////////////////////

    // all the functions in the module
    function_entry_t* functions;

    struct {
        System_Type key;
        MIR_item_t value;
    }* types;

    // the mir context relating to this stack
    MIR_context_t context;

    //////////////////////////////////////////////////////////////////////////////////

    // runtime functions
    MIR_item_t gc_new_proto;
    MIR_item_t gc_new_func;

    MIR_item_t get_array_type_proto;
    MIR_item_t get_array_type_func;

    MIR_item_t setjmp_proto;
    MIR_item_t setjmp_func;

    MIR_item_t longjmp_proto;
    MIR_item_t longjmp_func;
} jit_context_t;

static MIR_reg_t new_reg(jit_context_t* ctx, System_Type type) {
    // create the name
    char name[64];
    snprintf(name, sizeof(name), "s%d", ++ctx->name_gen);

    // create the reg
    MIR_reg_t reg;
    if (type_is_integer(type) || !type->IsValueType) {
        // This is an integer or a reference type
        reg = MIR_new_func_reg(ctx->context, ctx->func->u.func, MIR_T_I64, name);
    } else if (type == tSystem_Single) {
        // This is a float
        reg = MIR_new_func_reg(ctx->context, ctx->func->u.func, MIR_T_F, name);
    } else if (type == tSystem_Double) {
        // This is a double
        reg = MIR_new_func_reg(ctx->context, ctx->func->u.func, MIR_T_D, name);
    } else {
        // This is a value type, allocate a big enough space for it at the start
        reg = MIR_new_func_reg(ctx->context, ctx->func->u.func, MIR_T_I64, name);
        MIR_prepend_insn(ctx->context, ctx->func,
                         MIR_new_insn(ctx->context, MIR_ALLOCA,
                                      MIR_new_reg_op(ctx->context, reg),
                                      MIR_new_int_op(ctx->context, type->StackSize)));
    }

    return reg;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Stack helpers
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static err_t stack_pop(jit_context_t* ctx, System_Type* type, MIR_reg_t* reg) {
    err_t err = NO_ERROR;

    // pop the entry
    CHECK(arrlen(ctx->stack.entries) > 0);
    stack_entry_t entry = arrpop(ctx->stack.entries);
    *reg = entry.reg;
    *type = entry.type;

cleanup:
    return err;
}

static err_t stack_push(jit_context_t* ctx, System_Type type, MIR_reg_t* out_reg) {
    err_t err = NO_ERROR;

    stack_entry_t entry = {
        .type = type
    };

    // Make sure we don't exceed the stack depth
    CHECK(arrlen(ctx->stack.entries) < ctx->method_info->MethodBody->MaxStackSize);

    // create the reg
    MIR_reg_t reg = new_reg(ctx, type);

    // set the actual op
    entry.reg = reg;

    // give out if needed
    if (out_reg != NULL) {
        *out_reg = entry.reg;
    }

    // append to the stack
    arrpush(ctx->stack.entries, entry);

cleanup:
    return err;
}

static stack_t stack_snapshot(jit_context_t* ctx) {
    stack_t snapshot = { 0 };
    arrsetlen(snapshot.entries, arrlen(ctx->stack.entries));
    memcpy(snapshot.entries, ctx->stack.entries, arrlen(ctx->stack.entries) * sizeof(stack_entry_t));
    return snapshot;
}

static err_t stack_merge(jit_context_t* ctx, stack_t* stack, bool allow_change) {
    err_t err = NO_ERROR;

    // we must have the same number of slots
    CHECK(arrlen(stack->entries) == arrlen(ctx->stack.entries));

    // now merge it
    for (int i = 0; i < arrlen(stack->entries); i++) {
        System_Type T = ctx->stack.entries[i].type;
        System_Type S = stack->entries[i].type;

        // figure the new value that should be in here
        System_Type U = NULL;
        if (type_is_verifier_assignable_to(T, S)) {
            U = S;
        } else if (type_is_verifier_assignable_to(S, T)) {
            U = T;
        }
        // TODO: closest common subtype of S and T
        else {
            CHECK_FAIL();
        }

        if (allow_change) {
            // for forward jumps we allow to merge properly
            stack->entries[i].type = U;
        } else {
            // for backwards jumps we are going to check the stack
            // does not change after merging
            CHECK(stack->entries[i].type == U);
        }
    }

cleanup:
    return err;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Name formatting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void print_full_type_name(System_Type type, FILE* file) {
    if (type->DeclaringType != NULL) {
        print_full_type_name(type, file);
        fputc('+', file);
    } else {
        if (type->Namespace->Length > 0) {
            fprintf(file, "%U.", type->Namespace);
        }
    }
    fprintf(file, "%U", type->Name);
}

static void print_full_method_name(System_Reflection_MethodInfo method, FILE* name) {
    print_full_type_name(method->DeclaringType, name);
    fputc(':', name);
    fputc(':', name);
    fprintf(name, "%U", method->Name);
    fputc('(', name);
    for (int i = 0; i < method->Parameters->Length; i++) {
        print_full_type_name(method->Parameters->Data[i]->ParameterType, name);
        if (i + 1 != method->Parameters->Length) {
            fputc(',', name);
        }
    }
    fputc(')', name);
}

static err_t prepare_method_signature(jit_context_t* ctx, System_Reflection_MethodInfo method) {
    err_t err = NO_ERROR;

    FILE* proto_name = fcreate();
    print_full_method_name(method, proto_name);
    fprintf(proto_name, "$proto");
    fputc('\0', proto_name);

    FILE* func_name = fcreate();
    print_full_method_name(method, func_name);
    fputc('\0', func_name);

    size_t nres = 0;
    MIR_type_t res_type = 0;

    MIR_var_t* vars = NULL;

    if (method->ReturnType != NULL) {
        res_type = get_mir_type(method->ReturnType);
        if (res_type == MIR_T_BLK) {
            CHECK_FAIL("TODO: RBLK return value");
        }
        nres = 1;
    }

    if (!method_is_static(method)) {
        MIR_var_t var = {
        .name = "",
        .type = get_mir_type(method->DeclaringType),
        };
        if (var.type == MIR_T_BLK) {
            var.type = MIR_T_P;
        }
        arrpush(vars, var);
    }

    for (int i = 0; i < method->Parameters->Length; i++) {
        MIR_var_t var = {
            .name = "",
            .type = get_mir_type(method->Parameters->Data[i]->ParameterType),
        };
        if (var.type == MIR_T_BLK) {
            var.size = method->Parameters->Data[i]->ParameterType->StackSize;
        }
        arrpush(vars, var);
    }

    // create the proto def
    MIR_item_t proto = MIR_new_proto_arr(ctx->context, proto_name->buffer, nres, &res_type, arrlen(vars), vars);
    MIR_item_t forward = MIR_new_forward(ctx->context, func_name->buffer);

    function_entry_t entry = (function_entry_t){
        .key = method,
        .proto = proto,
        .forward = forward
    };
    hmputs(ctx->functions, entry);

cleanup:
    // free the vars
    arrfree(vars);

    // free the name
    fclose(proto_name);
    fclose(func_name);

    return err;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Method jitting
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static err_t jit_compare(jit_context_t* ctx, MIR_insn_code_t code) {
    err_t err = NO_ERROR;

    MIR_reg_t value2_reg;
    MIR_reg_t value1_reg;
    System_Type value2_type;
    System_Type value1_type;
    CHECK_AND_RETHROW(stack_pop(ctx, &value2_type, &value2_reg));
    CHECK_AND_RETHROW(stack_pop(ctx, &value1_type, &value1_reg));

    MIR_reg_t result_reg;
    CHECK_AND_RETHROW(stack_push(ctx, tSystem_Int32, &result_reg));

    if (value1_type == tSystem_Int32) {
        if (value2_type == tSystem_Int32) {
            MIR_append_insn(ctx->context, ctx->func,
                            MIR_new_insn(ctx->context, code + 1,
                                         MIR_new_reg_op(ctx->context, result_reg),
                                         MIR_new_reg_op(ctx->context, value1_reg),
                                         MIR_new_reg_op(ctx->context, value2_reg)));
        } else if (value2_type == tSystem_IntPtr) {
            MIR_append_insn(ctx->context, ctx->func,
                            MIR_new_insn(ctx->context, code,
                                         MIR_new_reg_op(ctx->context, result_reg),
                                         MIR_new_reg_op(ctx->context, value1_reg),
                                         MIR_new_reg_op(ctx->context, value2_reg)));
        } else {
            CHECK_FAIL();
        }
    } else if (value1_type == tSystem_Int64) {
        CHECK(value2_type == tSystem_Int64);
        MIR_append_insn(ctx->context, ctx->func,
                        MIR_new_insn(ctx->context, code,
                                     MIR_new_reg_op(ctx->context, result_reg),
                                     MIR_new_reg_op(ctx->context, value1_reg),
                                     MIR_new_reg_op(ctx->context, value2_reg)));
    } else if (value1_type == tSystem_IntPtr) {
        CHECK(value2_type == tSystem_Int32 || value2_type == tSystem_IntPtr);
        MIR_append_insn(ctx->context, ctx->func,
                        MIR_new_insn(ctx->context, code,
                                     MIR_new_reg_op(ctx->context, result_reg),
                                     MIR_new_reg_op(ctx->context, value1_reg),
                                     MIR_new_reg_op(ctx->context, value2_reg)));
    } else if (value1_type == tSystem_Single) {
        if (value2_type == tSystem_Single) {
            MIR_append_insn(ctx->context, ctx->func,
                            MIR_new_insn(ctx->context, code + 2,
                                         MIR_new_reg_op(ctx->context, result_reg),
                                         MIR_new_reg_op(ctx->context, value1_reg),
                                         MIR_new_reg_op(ctx->context, value2_reg)));
        } else if (value2_type == tSystem_Double) {
            // implicit conversion float->double
            MIR_reg_t value1_double_reg = new_reg(ctx, tSystem_Double);
            MIR_append_insn(ctx->context, ctx->func,
                            MIR_new_insn(ctx->context, MIR_F2D,
                                         MIR_new_reg_op(ctx->context, value1_double_reg),
                                         MIR_new_reg_op(ctx->context, value1_reg)));
            MIR_append_insn(ctx->context, ctx->func,
                            MIR_new_insn(ctx->context, code + 3,
                                         MIR_new_reg_op(ctx->context, result_reg),
                                         MIR_new_reg_op(ctx->context, value1_double_reg),
                                         MIR_new_reg_op(ctx->context, value2_reg)));
        } else {
            CHECK_FAIL();
        }
    } else if (value1_type == tSystem_Double) {
        if (value2_type == tSystem_Single) {
            // implicit conversion float->double
            MIR_reg_t value2_double_reg = new_reg(ctx, tSystem_Double);
            MIR_append_insn(ctx->context, ctx->func,
                            MIR_new_insn(ctx->context, MIR_F2D,
                                         MIR_new_reg_op(ctx->context, value2_double_reg),
                                         MIR_new_reg_op(ctx->context, value2_reg)));
            MIR_append_insn(ctx->context, ctx->func,
                            MIR_new_insn(ctx->context, code + 3,
                                         MIR_new_reg_op(ctx->context, result_reg),
                                         MIR_new_reg_op(ctx->context, value1_reg),
                                         MIR_new_reg_op(ctx->context, value2_double_reg)));
        } else if (value2_type == tSystem_Double) {
            MIR_append_insn(ctx->context, ctx->func,
                            MIR_new_insn(ctx->context, code + 3,
                                         MIR_new_reg_op(ctx->context, result_reg),
                                         MIR_new_reg_op(ctx->context, value1_reg),
                                         MIR_new_reg_op(ctx->context, value2_reg)));
        } else {
            CHECK_FAIL();
        }
    } else if (!value1_type->IsValueType) {
        CHECK(!value2_type->IsValueType);
        MIR_append_insn(ctx->context, ctx->func,
                        MIR_new_insn(ctx->context, code,
                                     MIR_new_reg_op(ctx->context, result_reg),
                                     MIR_new_reg_op(ctx->context, value1_reg),
                                     MIR_new_reg_op(ctx->context, value2_reg)));
    } else {
        // this is an invalid conversion
        CHECK_FAIL();
    }

cleanup:
    return err;
}

static err_t jit_branch_point(jit_context_t* ctx, int il_offset, int il_target, MIR_label_t* label) {
    err_t err = NO_ERROR;

    // resolve the label
    if (il_target >= il_offset) {
        // forward jump, check if someone already jumps to there
        int i = hmgeti(ctx->pc_to_stack_snapshot, il_target);
        if (i == -1) {
            // nope, we are the first
            *label = MIR_new_label(ctx->context);
            stack_snapshot_t snapshot = {
                    .key = il_target,
                    .label = *label,
                    .stack = stack_snapshot(ctx),
            };
            hmputs(ctx->pc_to_stack_snapshot, snapshot);
        } else {
            // yes, we need to merge with it, we can allow changes because we did not
            // arrive to that part of scanning yet
            stack_t snapshot = ctx->pc_to_stack_snapshot[i].stack;
            CHECK_AND_RETHROW(stack_merge(ctx, &snapshot, true));
            *label = ctx->pc_to_stack_snapshot[i].label;
        }
    } else {
        // backwards jump, get the stack there and validate it, we can not
        // actually merge the stack because we already scanned through that
        // part of the code
        int i = hmgeti(ctx->pc_to_stack_snapshot, il_target);
        CHECK(i != -1);
        stack_t snapshot = ctx->pc_to_stack_snapshot[i].stack;
        CHECK_AND_RETHROW(stack_merge(ctx, &snapshot, false));
        *label = ctx->pc_to_stack_snapshot[i].label;
    }

cleanup:
    return err;
}

static err_t jit_compare_branch(jit_context_t* ctx, MIR_insn_code_t code, int il_offset, int il_target) {
    err_t err = NO_ERROR;

    // get the values
    MIR_reg_t value2_reg;
    MIR_reg_t value1_reg;
    System_Type value2_type;
    System_Type value1_type;
    CHECK_AND_RETHROW(stack_pop(ctx, &value2_type, &value2_reg));
    CHECK_AND_RETHROW(stack_pop(ctx, &value1_type, &value1_reg));

    // get the label
    MIR_label_t label;
    CHECK_AND_RETHROW(jit_branch_point(ctx, il_offset, il_target, &label));

    if (value1_type == tSystem_Int32) {
        if (value2_type == tSystem_Int32) {
            MIR_append_insn(ctx->context, ctx->func,
                            MIR_new_insn(ctx->context, code + 1,
                                         MIR_new_label_op(ctx->context, label),
                                         MIR_new_reg_op(ctx->context, value1_reg),
                                         MIR_new_reg_op(ctx->context, value2_reg)));
        } else if (value2_type == tSystem_IntPtr) {
            MIR_append_insn(ctx->context, ctx->func,
                            MIR_new_insn(ctx->context, code,
                                         MIR_new_label_op(ctx->context, label),
                                         MIR_new_reg_op(ctx->context, value1_reg),
                                         MIR_new_reg_op(ctx->context, value2_reg)));
        } else {
            CHECK_FAIL();
        }
    } else if (value1_type == tSystem_Int64) {
        CHECK(value2_type == tSystem_Int64);
        MIR_append_insn(ctx->context, ctx->func,
                        MIR_new_insn(ctx->context, code,
                                     MIR_new_label_op(ctx->context, label),
                                     MIR_new_reg_op(ctx->context, value1_reg),
                                     MIR_new_reg_op(ctx->context, value2_reg)));
    } else if (value1_type == tSystem_IntPtr) {
        CHECK(value2_type == tSystem_Int32 || value2_type == tSystem_IntPtr);
        MIR_append_insn(ctx->context, ctx->func,
                        MIR_new_insn(ctx->context, code,
                                     MIR_new_label_op(ctx->context, label),
                                     MIR_new_reg_op(ctx->context, value1_reg),
                                     MIR_new_reg_op(ctx->context, value2_reg)));
    } else if (value1_type == tSystem_Single) {
        if (value2_type == tSystem_Single) {
            MIR_append_insn(ctx->context, ctx->func,
                            MIR_new_insn(ctx->context, code + 2,
                                         MIR_new_label_op(ctx->context, label),
                                         MIR_new_reg_op(ctx->context, value1_reg),
                                         MIR_new_reg_op(ctx->context, value2_reg)));
        } else if (value2_type == tSystem_Double) {
            // implicit conversion float->double
            MIR_reg_t value1_double_reg = new_reg(ctx, tSystem_Double);
            MIR_append_insn(ctx->context, ctx->func,
                            MIR_new_insn(ctx->context, MIR_F2D,
                                         MIR_new_reg_op(ctx->context, value1_double_reg),
                                         MIR_new_reg_op(ctx->context, value1_reg)));
            MIR_append_insn(ctx->context, ctx->func,
                            MIR_new_insn(ctx->context, code + 3,
                                         MIR_new_label_op(ctx->context, label),
                                         MIR_new_reg_op(ctx->context, value1_double_reg),
                                         MIR_new_reg_op(ctx->context, value2_reg)));
        } else {
            CHECK_FAIL();
        }
    } else if (value1_type == tSystem_Double) {
        if (value2_type == tSystem_Single) {
            // implicit conversion float->double
            MIR_reg_t value2_double_reg = new_reg(ctx, tSystem_Double);
            MIR_append_insn(ctx->context, ctx->func,
                            MIR_new_insn(ctx->context, MIR_F2D,
                                         MIR_new_reg_op(ctx->context, value2_double_reg),
                                         MIR_new_reg_op(ctx->context, value2_reg)));
            MIR_append_insn(ctx->context, ctx->func,
                            MIR_new_insn(ctx->context, code + 3,
                                         MIR_new_label_op(ctx->context, label),
                                         MIR_new_reg_op(ctx->context, value1_reg),
                                         MIR_new_reg_op(ctx->context, value2_double_reg)));
        } else if (value2_type == tSystem_Double) {
            MIR_append_insn(ctx->context, ctx->func,
                            MIR_new_insn(ctx->context, code + 3,
                                         MIR_new_label_op(ctx->context, label),
                                         MIR_new_reg_op(ctx->context, value1_reg),
                                         MIR_new_reg_op(ctx->context, value2_reg)));
        } else {
            CHECK_FAIL();
        }
    } else if (!value1_type->IsValueType) {
        CHECK(!value2_type->IsValueType);
        MIR_append_insn(ctx->context, ctx->func,
                        MIR_new_insn(ctx->context, code,
                                     MIR_new_label_op(ctx->context, label),
                                     MIR_new_reg_op(ctx->context, value1_reg),
                                     MIR_new_reg_op(ctx->context, value2_reg)));
    } else {
        // this is an invalid conversion
        CHECK_FAIL();
    }

cleanup:
    return err;
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "UnreachableCode"
static err_t jit_method(jit_context_t* ctx, System_Reflection_MethodInfo method) {
    err_t err = NO_ERROR;

    // preprae the context for the current method
    ctx->method_info = method;
    ctx->name_gen = 0;

    System_Reflection_MethodBody body = method->MethodBody;
    System_Reflection_Assembly assembly = method->Module->Assembly;

    FILE* method_name = fcreate();
    print_full_method_name(method, method_name);
    fputc('\0', method_name);

    size_t nres = 0;
    MIR_type_t res_type = 0;

    MIR_var_t* vars = NULL;
    FILE** var_names = NULL;

    if (method->ReturnType != NULL) {
        res_type = get_mir_type(method->ReturnType);
        if (res_type == MIR_T_BLK) {
            CHECK_FAIL("TODO: RBLK return value");
        }
        nres = 1;
    }

    if (!method_is_static(method)) {
        FILE* var_name = fcreate();
        fprintf(var_name, "a%d", arrlen(vars));
        fputc('\0', var_name);
        MIR_var_t var = {
            .name = var_name->buffer,
            .type = get_mir_type(method->DeclaringType),
        };
        if (var.type == MIR_T_BLK) {
            var.type = MIR_T_P;
        }
        arrpush(var_names, var_name);
        arrpush(vars, var);
    }

    for (int i = 0; i < method->Parameters->Length; i++) {
        FILE* var_name = fcreate();
        fprintf(var_name, "a%d", arrlen(vars));
        fputc('\0', var_name);
        MIR_var_t var = {
            .name = var_name->buffer,
            .type = get_mir_type(method->Parameters->Data[i]->ParameterType),
        };
        if (var.type == MIR_T_BLK) {
            var.size = method->Parameters->Data[i]->ParameterType->StackSize;
        }
        arrpush(var_names, var_name);
        arrpush(vars, var);
    }

    ctx->func = MIR_new_func_arr(ctx->context, method_name->buffer, nres, &res_type, arrlen(vars), vars);

    int il_offset = 0;
    while (il_offset < body->Il->Length) {
        // create a snapshot of the stack, if we already have a snapshot
        // of this verify it is the same (we will get a snapshot if we have
        // a forward jump)
        MIR_insn_t cur_label;
        int stacki = hmgeti(ctx->pc_to_stack_snapshot, il_offset);
        if (stacki != -1) {
            // verify it is the same
            stack_t snapshot = ctx->pc_to_stack_snapshot[stacki].stack;
            cur_label = ctx->pc_to_stack_snapshot[stacki].label;
            CHECK_AND_RETHROW(stack_merge(ctx, &snapshot, true));
        } else {
            // take snapshot
            cur_label = MIR_new_label(ctx->context);
            stack_snapshot_t snapshot = {
                .key = il_offset,
                .label = cur_label,
                .stack = stack_snapshot(ctx)
            };
            hmputs(ctx->pc_to_stack_snapshot, snapshot);
        }
        MIR_append_insn(ctx->context, ctx->func, cur_label);

        // get the opcode value
        uint16_t opcode_value = (REFPRE << 8) | body->Il->Data[il_offset++];

        // get the actual opcode
        opcode_t opcode = g_dotnet_opcode_lookup[opcode_value];
        CHECK_ERROR(opcode != CEE_INVALID, ERROR_INVALID_OPCODE);

        // handle opcodes with special prefix
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

        // get the opcode info
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
                operand_i32 = *(int32_t*)&body->Il->Data[il_offset];
                il_offset += sizeof(int32_t);
                operand_i32 += il_offset;
            } break;
            case OPCODE_OPERAND_InlineField: {
                token_t value = *(token_t*)&body->Il->Data[il_offset];
                il_offset += sizeof(token_t);
                operand_field = assembly_get_field_by_token(assembly, value);
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
                operand_i32 = *(int8_t*)&body->Il->Data[il_offset];
                il_offset += sizeof(int8_t);
                operand_i32 += il_offset;
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
        // Handle the opcode
        //--------------------------------------------------------------------------------------------------------------

        switch (opcode) {
            case CEE_NOP: break;

            case CEE_LDARG_0:
            case CEE_LDARG_1:
            case CEE_LDARG_2:
            case CEE_LDARG_3: operand_i32 = opcode - CEE_LDARG_0;
            case CEE_LDARG_S:
            case CEE_LDARG: {
                // the method_name to resolve it
                char arg_name[64];
                snprintf(arg_name, sizeof(arg_name), "a%d", operand_i32);

                // resolve the type
                System_Type arg_type = NULL;
                if (!method_is_static(method)) {
                    if (operand_i32 == 0) {
                        arg_type = method->DeclaringType;
                        if (arg_type->IsValueType) {
                            CHECK_FAIL("TODO: value type this");
                        }
                    }
                    operand_i32--;
                }

                if (arg_type == NULL) {
                    CHECK(operand_i32 < method->Parameters->Length);
                    arg_type = method->Parameters->Data[operand_i32]->ParameterType;
                }

                // the register containing the value
                MIR_reg_t arg_reg = MIR_reg(ctx->context, arg_name, ctx->func->u.func);

                // Get the stack type of the arg
                System_Type arg_stack_type = type_get_intermediate_type(arg_type);

                // push it
                MIR_reg_t value_reg;
                CHECK_AND_RETHROW(stack_push(ctx, arg_stack_type, &value_reg));

                // for register and reference types we can just copy it
                if (
                    !arg_stack_type->IsValueType ||
                    arg_stack_type == tSystem_Int32 ||
                    arg_stack_type == tSystem_Int64 ||
                    arg_stack_type == tSystem_IntPtr
                ) {
                    MIR_append_insn(ctx->context, ctx->func,
                                    MIR_new_insn(ctx->context, MIR_MOV,
                                                 MIR_new_reg_op(ctx->context, value_reg),
                                                 MIR_new_reg_op(ctx->context, arg_reg)));
                } else if (arg_stack_type == tSystem_Single) {
                    MIR_append_insn(ctx->context, ctx->func,
                                    MIR_new_insn(ctx->context, MIR_FMOV,
                                                 MIR_new_reg_op(ctx->context, value_reg),
                                                 MIR_new_reg_op(ctx->context, arg_reg)));
                } else if (arg_stack_type == tSystem_Double) {
                    MIR_append_insn(ctx->context, ctx->func,
                                    MIR_new_insn(ctx->context, MIR_DMOV,
                                                 MIR_new_reg_op(ctx->context, value_reg),
                                                 MIR_new_reg_op(ctx->context, arg_reg)));
                } else {
                    CHECK_FAIL("TODO: copy arg to stack");
                }
            } break;

            case CEE_LDC_I4_M1:
            case CEE_LDC_I4_0:
            case CEE_LDC_I4_1:
            case CEE_LDC_I4_2:
            case CEE_LDC_I4_3:
            case CEE_LDC_I4_4:
            case CEE_LDC_I4_5:
            case CEE_LDC_I4_6:
            case CEE_LDC_I4_7:
            case CEE_LDC_I4_8: operand_i32 = (int32_t)opcode - CEE_LDC_I4_0;
            case CEE_LDC_I4_S:
            case CEE_LDC_I4: {
                MIR_reg_t sr;
                CHECK_AND_RETHROW(stack_push(ctx, tSystem_Int32, &sr));
                MIR_append_insn(ctx->context, ctx->func,
                                MIR_new_insn(ctx->context, MIR_MOV,
                                             MIR_new_reg_op(ctx->context, sr),
                                             MIR_new_int_op(ctx->context, operand_i32)));
            } break;

            //----------------------------------------------------------------------------------------------------------
            // ldfld - load field of an object
            //----------------------------------------------------------------------------------------------------------
            case CEE_LDFLD: {
                // get the object instance
                System_Type obj_type;
                MIR_reg_t obj_reg;
                CHECK_AND_RETHROW(stack_pop(ctx, &obj_type, &obj_reg));

                // validate the field is part of the object
                System_Type base = obj_type;
                while (base != NULL && base != operand_field->DeclaringType) {
                    base = base->BaseType;
                }
                CHECK(base != NULL);

                // TODO: check accessibility

                // TODO: does the runtime actually use ldfld for static fields?
                CHECK(!field_is_static(operand_field));

                // make sure the field is compatible
                CHECK(type_is_compatible_with(obj_type, operand_field->DeclaringType));

                // Get the field type
                System_Type field_stack_type = type_get_intermediate_type(operand_field->FieldType);
                System_Type field_type = type_get_underlying_type(operand_field->FieldType);

                // push it
                MIR_reg_t value_reg;
                CHECK_AND_RETHROW(stack_push(ctx, field_stack_type, &value_reg));

                if (
                    !field_stack_type->IsValueType ||
                    field_stack_type == tSystem_Int32 ||
                    field_stack_type == tSystem_Int64 ||
                    field_stack_type == tSystem_IntPtr
                ) {
                    // we need to extend this properly if the field is smaller
                    // than an int32 (because we are going to load into an int32
                    // essentially)
                    MIR_insn_code_t insn = MIR_MOV;
                    if (field_type == tSystem_SByte || field_type == tSystem_Boolean) {
                        insn = MIR_EXT8;
                    } else if (field_type == tSystem_Byte) {
                        insn = MIR_UEXT8;
                    } else if (field_type == tSystem_Int16) {
                        insn = MIR_EXT16;
                    } else if (field_type == tSystem_UInt16 || field_type == tSystem_Char) {
                        insn = MIR_UEXT16;
                    } else if (field_type == tSystem_Int32) {
                        insn = MIR_EXT32;
                    } else if (field_type == tSystem_UInt32) {
                        insn = MIR_UEXT32;
                    }

                    // integer type
                    MIR_append_insn(ctx->context, ctx->func,
                                    MIR_new_insn(ctx->context, insn,
                                                 MIR_new_reg_op(ctx->context, value_reg),
                                                 MIR_new_mem_op(ctx->context,
                                                                get_mir_type(operand_field->FieldType),
                                                                (int)operand_field->MemoryOffset,
                                                                obj_reg, 0, 1)));
                } else if (field_stack_type == tSystem_Single) {
                    MIR_append_insn(ctx->context, ctx->func,
                                    MIR_new_insn(ctx->context, MIR_FMOV,
                                                 MIR_new_reg_op(ctx->context, value_reg),
                                                 MIR_new_mem_op(ctx->context,
                                                                MIR_T_F,
                                                                (int)operand_field->MemoryOffset,
                                                                obj_reg, 0, 1)));
                } else if (field_stack_type == tSystem_Double) {
                    MIR_append_insn(ctx->context, ctx->func,
                                    MIR_new_insn(ctx->context, MIR_DMOV,
                                                 MIR_new_reg_op(ctx->context, value_reg),
                                                 MIR_new_mem_op(ctx->context,
                                                                MIR_T_D,
                                                                (int)operand_field->MemoryOffset,
                                                                obj_reg, 0, 1)));
                } else {
                    CHECK_FAIL("memcpy field");
                }
            } break;

            //----------------------------------------------------------------------------------------------------------
            // stfld - store into a field of an object
            //----------------------------------------------------------------------------------------------------------
            case CEE_STFLD: {
                // get the values
                MIR_reg_t obj_reg;
                MIR_reg_t value_reg;
                System_Type obj_type;
                System_Type value_type;
                CHECK_AND_RETHROW(stack_pop(ctx, &value_type, &value_reg));
                CHECK_AND_RETHROW(stack_pop(ctx, &obj_type, &obj_reg));

                // validate the field is part of the object
                System_Type base = obj_type;
                while (base != NULL && base != operand_field->DeclaringType) {
                    base = base->BaseType;
                }
                CHECK(base != NULL);

                System_Type field_type = type_get_underlying_type(operand_field->FieldType);

                // TODO: check field access

                // TODO: does the runtime actually use ldfld for static fields?
                CHECK(!field_is_static(operand_field));

                // validate the assignability
                CHECK(type_is_verifier_assignable_to(value_type, operand_field->FieldType));

                if (
                    !value_type->IsValueType ||
                    value_type == tSystem_Int32 ||
                    value_type == tSystem_Int64 ||
                    value_type == tSystem_IntPtr
                ) {
                    // integer type
                    MIR_append_insn(ctx->context, ctx->func,
                                    MIR_new_insn(ctx->context, MIR_MOV,
                                                 MIR_new_mem_op(ctx->context,
                                                                get_mir_type(operand_field->FieldType),
                                                                (int)operand_field->MemoryOffset,
                                                                obj_reg, 0, 1),
                                                 MIR_new_reg_op(ctx->context, value_reg)));
                } else if (value_type == tSystem_Single) {
                    MIR_append_insn(ctx->context, ctx->func,
                                    MIR_new_insn(ctx->context, MIR_FMOV,
                                                 MIR_new_mem_op(ctx->context,
                                                                MIR_T_F,
                                                                (int)operand_field->MemoryOffset,
                                                                obj_reg, 0, 1),
                                                 MIR_new_reg_op(ctx->context, value_reg)));
                } else if (value_type == tSystem_Double) {
                    MIR_append_insn(ctx->context, ctx->func,
                                    MIR_new_insn(ctx->context, MIR_DMOV,
                                                 MIR_new_mem_op(ctx->context,
                                                                MIR_T_D,
                                                                (int)operand_field->MemoryOffset,
                                                                obj_reg, 0, 1),
                                                 MIR_new_reg_op(ctx->context, value_reg)));
                } else {
                    CHECK_FAIL("memcpy field");
                }
            } break;

            //----------------------------------------------------------------------------------------------------------
            // dup - duplicate the top value of the stack
            //----------------------------------------------------------------------------------------------------------
            case CEE_DUP: {
                // get the top value
                MIR_reg_t top_reg;
                System_Type top_type;
                CHECK_AND_RETHROW(stack_pop(ctx, &top_type, &top_reg));

                // create new two values
                MIR_reg_t value_1;
                MIR_reg_t value_2;
                CHECK_AND_RETHROW(stack_push(ctx, top_type, &value_1));
                CHECK_AND_RETHROW(stack_push(ctx, top_type, &value_2));

                if (
                    !top_type->IsValueType ||
                    type_is_integer(top_type) ||
                    top_type == tSystem_Single ||
                    top_type == tSystem_Double
                ) {
                    // normal value, copy the two regs
                    MIR_append_insn(ctx->context, ctx->func,
                                    MIR_new_insn(ctx->context, MIR_MOV,
                                                 MIR_new_reg_op(ctx->context, value_1),
                                                 MIR_new_reg_op(ctx->context, top_reg)));
                    MIR_append_insn(ctx->context, ctx->func,
                                    MIR_new_insn(ctx->context, MIR_MOV,
                                                 MIR_new_reg_op(ctx->context, value_2),
                                                 MIR_new_reg_op(ctx->context, top_reg)));
                } else {
                    // only copy the second value, we can move the pointer
                    // to the second one because we are essentially SSA
                    MIR_append_insn(ctx->context, ctx->func,
                                    MIR_new_insn(ctx->context, MIR_MOV,
                                                 MIR_new_reg_op(ctx->context, value_1),
                                                 MIR_new_reg_op(ctx->context, top_reg)));

                    CHECK_FAIL("TODO: copy the stack value");
                }

            } break;

            //----------------------------------------------------------------------------------------------------------
            // call - call a method
            //----------------------------------------------------------------------------------------------------------
            case CEE_CALL: {
                System_Type ret_type = type_get_underlying_type(operand_method->ReturnType);

                // count the amount of arguments, +1 if we have a this
                int arg_count = operand_method->Parameters->Length;

                // TODO: the method must be accessible from the call size.

                // validate the method is not abstract
                CHECK(!method_is_abstract(operand_method));

                // prepare array of all the operands
                // 1st is the prototype
                // 2nd is the reference
                // 3rd is return type optionally
                // 4th is this type optionally
                // Rest are the arguments
                size_t other_args = 2;
                if (ret_type != NULL) other_args++;
                if (!method_is_static(operand_method)) other_args++;
                MIR_op_t arg_ops[other_args + arg_count];

                // pop all the arguments from the stack
                int i;
                for (i = arg_count + other_args - 1; i >= other_args; i--) {
                    MIR_reg_t arg_reg;
                    System_Type arg_type;
                    CHECK_AND_RETHROW(stack_pop(ctx, &arg_type, &arg_reg));
                    arg_ops[i] = MIR_new_reg_op(ctx->context, arg_reg);

                    // verify a normal argument
                    CHECK(type_is_verifier_assignable_to(
                            type_get_verification_type(arg_type), operand_method->Parameters->Data[i - other_args]->ParameterType));
                }

                // handle the this argument
                if (!method_is_static(operand_method)) {
                    MIR_reg_t arg_reg;
                    System_Type arg_type;
                    CHECK_AND_RETHROW(stack_pop(ctx, &arg_type, &arg_reg));
                    arg_ops[i] = MIR_new_reg_op(ctx->context, arg_reg);

                    // verify a normal argument
                    CHECK(type_is_verifier_assignable_to(
                            type_get_verification_type(arg_type), operand_method->DeclaringType));
                }

                // get the MIR signature and address
                int funci = hmgeti(ctx->functions, operand_method);
                CHECK(i != -1);
                arg_ops[0] = MIR_new_ref_op(ctx->context, ctx->functions[funci].proto);
                arg_ops[1] = MIR_new_ref_op(ctx->context, ctx->functions[funci].forward);

                // emit the IR
                if (operand_method->ReturnType != NULL) {
                    MIR_reg_t ret_reg;
                    CHECK_AND_RETHROW(stack_push(ctx, type_get_intermediate_type(operand_method->ReturnType), &ret_reg));

                    // Has return argument, handle it
                    if (
                        !ret_type->IsValueType ||
                        type_is_integer(ret_type) ||
                        ret_type == tSystem_Single ||
                        ret_type == tSystem_Double
                    ) {
                        // return argument is a simple register
                        arg_ops[2] = MIR_new_reg_op(ctx->context, ret_reg);
                        MIR_append_insn(ctx->context, ctx->func,
                                        MIR_new_insn_arr(ctx->context, MIR_CALL,
                                                         other_args + arg_count,
                                                         arg_ops));
                    } else {
                        CHECK_FAIL("TODO: pass to RBLK");
                    }
                } else {
                    // Does not have a return argument, no need to handle
                    MIR_append_insn(ctx->context, ctx->func,
                                    MIR_new_insn_arr(ctx->context, MIR_CALL,
                                                     other_args + arg_count,
                                                     arg_ops));
                }
            } break;

            //----------------------------------------------------------------------------------------------------------
            // ret - return from method
            //----------------------------------------------------------------------------------------------------------
            case CEE_RET: {
                // TODO: check
                System_Type method_ret_type = type_get_underlying_type(method->ReturnType);

                if (method_ret_type == NULL) {
                    // must be an empty stack, since we have no return value
                    CHECK(arrlen(ctx->stack.entries) == 0);

                    // there is no return value, just add a ret
                    MIR_append_insn(ctx->context, ctx->func,
                                    MIR_new_ret_insn(ctx->context, 0));
                } else {
                    // pop the return from the stack
                    MIR_reg_t ret_arg;
                    System_Type ret_type;
                    CHECK_AND_RETHROW(stack_pop(ctx, &ret_type, &ret_arg));

                    // verify the stack is empty
                    CHECK(arrlen(ctx->stack.entries) == 0);

                    // verify the IL
                    CHECK(type_is_verifier_assignable_to(ret_type, method->ReturnType));

                    // handle it at the IR level
                    if (
                        !ret_type->IsValueType ||
                        ret_type == tSystem_Int32 ||
                        ret_type == tSystem_Int64 ||
                        ret_type == tSystem_IntPtr ||
                        ret_type == tSystem_Single ||
                        ret_type == tSystem_Double
                    ) {
                        // it is stored in a register directly, just return it
                        MIR_append_insn(ctx->context, ctx->func,
                                        MIR_new_ret_insn(ctx->context, 1,
                                                         MIR_new_reg_op(ctx->context, ret_arg)));
                    } else {
                        // this is a big struct, copy it to the return block
                        CHECK_FAIL("TODO: copy to RBLK");
                    }
                }
            } break;

            //----------------------------------------------------------------------------------------------------------
            // brfalse - branch on false, null, or zero
            //----------------------------------------------------------------------------------------------------------
            case CEE_BRFALSE:
            case CEE_BRFALSE_S: {
                // get the value
                MIR_reg_t value_reg;
                System_Type value_type;
                CHECK_AND_RETHROW(stack_pop(ctx, &value_type, &value_reg));

                // get the label
                MIR_label_t label;
                CHECK_AND_RETHROW(jit_branch_point(ctx, il_offset, operand_i32, &label));

                // emit it properly
                if (value_type == tSystem_Int32) {
                    MIR_append_insn(ctx->context, ctx->func,
                                    MIR_new_insn(ctx->context, MIR_BFS,
                                                 MIR_new_label_op(ctx->context, label),
                                                 MIR_new_reg_op(ctx->context, value_reg)));
                } else if (
                    value_type == tSystem_Int64 ||
                    value_type == tSystem_IntPtr ||
                    !value_type->IsValueType
                ) {
                    MIR_append_insn(ctx->context, ctx->func,
                                    MIR_new_insn(ctx->context, MIR_BF,
                                                 MIR_new_label_op(ctx->context, label),
                                                 MIR_new_reg_op(ctx->context, value_reg)));
                } else {
                    CHECK_FAIL();
                }
            } break;

            //----------------------------------------------------------------------------------------------------------
            // brtrue - branch on false, null, or zero
            //----------------------------------------------------------------------------------------------------------
            case CEE_BRTRUE:
            case CEE_BRTRUE_S: {
                // get the value
                MIR_reg_t value_reg;
                System_Type value_type;
                CHECK_AND_RETHROW(stack_pop(ctx, &value_type, &value_reg));

                // get the label
                MIR_label_t label;
                CHECK_AND_RETHROW(jit_branch_point(ctx, il_offset, operand_i32, &label));

                // emit it properly
                if (value_type == tSystem_Int32) {
                    MIR_append_insn(ctx->context, ctx->func,
                                    MIR_new_insn(ctx->context, MIR_BTS,
                                                 MIR_new_label_op(ctx->context, label),
                                                 MIR_new_reg_op(ctx->context, value_reg)));
                } else if (
                    value_type == tSystem_Int64 ||
                    value_type == tSystem_IntPtr ||
                    !value_type->IsValueType
                ) {
                    MIR_append_insn(ctx->context, ctx->func,
                                    MIR_new_insn(ctx->context, MIR_BT,
                                                 MIR_new_label_op(ctx->context, label),
                                                 MIR_new_reg_op(ctx->context, value_reg)));
                } else {
                    CHECK_FAIL();
                }
            } break;

                //----------------------------------------------------------------------------------------------------------
            // bne.un - branch on not equal or unordered
            //----------------------------------------------------------------------------------------------------------
            case CEE_BNE_UN:
            case CEE_BNE_UN_S: {
                CHECK_AND_RETHROW(jit_compare_branch(ctx, MIR_BNE, il_offset, operand_i32));
            } break;

            //----------------------------------------------------------------------------------------------------------
            // newarr - create a zero-based, one-dimensional array
            //----------------------------------------------------------------------------------------------------------
            case CEE_NEWARR: {
                // get the number of elements
                MIR_reg_t num_elems_reg;
                System_Type num_elems_type;
                CHECK_AND_RETHROW(stack_pop(ctx, &num_elems_type, &num_elems_reg));

                // make sure it has a valid type
                CHECK(num_elems_type == tSystem_Int32);

                // get the item for the allocation
                int i = hmgeti(ctx->types, operand_type);
                CHECK(i != -1);
                MIR_item_t type_item = ctx->types[i].value;

                // push the array type
                MIR_reg_t array_reg;
                CHECK_AND_RETHROW(stack_push(ctx, get_array_type(operand_type), &array_reg));

                // calculate the size we are going to need:
                //  num_elems * sizeof(value_type) + sizeof(System.Array)
                MIR_reg_t size_reg = new_reg(ctx, tSystem_Int64);
                MIR_append_insn(ctx->context, ctx->func,
                                MIR_new_insn(ctx->context, MIR_MUL,
                                             MIR_new_reg_op(ctx->context, size_reg),
                                             MIR_new_reg_op(ctx->context, num_elems_reg),
                                             MIR_new_int_op(ctx->context, operand_type->StackSize)));
                MIR_append_insn(ctx->context, ctx->func,
                                MIR_new_insn(ctx->context, MIR_ADD,
                                             MIR_new_reg_op(ctx->context, size_reg),
                                             MIR_new_reg_op(ctx->context, size_reg),
                                             MIR_new_int_op(ctx->context, tSystem_Array->ManagedSize)));

                // TODO: somehow propagate that we need the static array type
                //       instead of using the dynamic method
                // get the type
                MIR_append_insn(ctx->context, ctx->func,
                                MIR_new_call_insn(ctx->context, 4,
                                                  MIR_new_ref_op(ctx->context, ctx->get_array_type_proto),
                                                  MIR_new_ref_op(ctx->context, ctx->get_array_type_func),
                                                  MIR_new_reg_op(ctx->context, array_reg),
                                                  MIR_new_ref_op(ctx->context, type_item)));

                // actually allocate it now
                MIR_append_insn(ctx->context, ctx->func,
                                MIR_new_call_insn(ctx->context, 5,
                                                  MIR_new_ref_op(ctx->context, ctx->gc_new_proto),
                                                  MIR_new_ref_op(ctx->context, ctx->gc_new_func),
                                                  MIR_new_reg_op(ctx->context, array_reg),
                                                  MIR_new_reg_op(ctx->context, array_reg),
                                                  MIR_new_reg_op(ctx->context, size_reg)));

                // Set the length of the array
                MIR_append_insn(ctx->context, ctx->func,
                                MIR_new_insn(ctx->context, MIR_MOV,
                                             MIR_new_mem_op(ctx->context,
                                                            MIR_T_I32,
                                                            offsetof(struct System_Array, Length),
                                                            array_reg,
                                                            0, 1),
                                             MIR_new_reg_op(ctx->context, num_elems_reg)));
            } break;

            //----------------------------------------------------------------------------------------------------------
            // stelem - store an element of an array
            //----------------------------------------------------------------------------------------------------------
            case CEE_STELEM_I1: operand_type = tSystem_SByte; goto cee_stelem;
            case CEE_STELEM_I2: operand_type = tSystem_Int16; goto cee_stelem;
            case CEE_STELEM_I4: operand_type = tSystem_Int32; goto cee_stelem;
            case CEE_STELEM_I8: operand_type = tSystem_Int64; goto cee_stelem;
            case CEE_STELEM_R4: operand_type = tSystem_Single; goto cee_stelem;
            case CEE_STELEM_R8: operand_type = tSystem_Double; goto cee_stelem;
            case CEE_STELEM_I: operand_type = tSystem_IntPtr; goto cee_stelem;
            case CEE_STELEM_REF: CHECK_FAIL("TODO: we need to figure this shit out");
            case CEE_STELEM:
            cee_stelem: {
                // pop all the values from the stack
                MIR_reg_t value_reg;
                MIR_reg_t index_reg;
                MIR_reg_t array_reg;
                System_Type value_type;
                System_Type index_type;
                System_Type array_type;
                CHECK_AND_RETHROW(stack_pop(ctx, &value_type, &value_reg));
                CHECK_AND_RETHROW(stack_pop(ctx, &index_type, &index_reg));
                CHECK_AND_RETHROW(stack_pop(ctx, &array_type, &array_reg));

                // this must be an array
                CHECK(array_type->IsArray);
                System_Type T = array_type->ElementType;

                // we need to implicitly truncate
                if (type_get_intermediate_type(T) == tSystem_Int32) {
                    value_type = operand_type;
                }

                // TODO: handle double->float and float->double implicit conversion

                // validate all the type stuff
                CHECK(type_is_array_element_compatible_with(value_type, operand_type));
                CHECK(type_is_array_element_compatible_with(operand_type, type_get_verification_type(T)));
                CHECK(index_type == tSystem_Int32);

                // TODO: insert range check for the array

                if (!T->IsValueType) {
                    // we need to use gc_update routine because this
                    // is a managed pointer
                    CHECK_FAIL("TODO: gc_update for stelem");
                } else if (type_is_integer(value_type)) {
                    // we can copy this in a single mov
                    MIR_append_insn(ctx->context, ctx->func,
                                    MIR_new_insn(ctx->context, MIR_MOV,
                                                 MIR_new_mem_op(ctx->context, get_mir_type(operand_type),
                                                                tSystem_Array->ManagedSize,
                                                                array_reg, index_reg, T->StackSize),
                                                 MIR_new_reg_op(ctx->context, value_reg)));
                } else if (value_type == tSystem_Single) {
                    // we can copy this in a single mov
                    MIR_append_insn(ctx->context, ctx->func,
                                    MIR_new_insn(ctx->context, MIR_FMOV,
                                                 MIR_new_mem_op(ctx->context, MIR_T_F,
                                                                tSystem_Array->ManagedSize,
                                                                array_reg, index_reg, T->StackSize),
                                                 MIR_new_reg_op(ctx->context, value_reg)));
                } else if (value_type == tSystem_Double) {
                    // we can copy this in a single mov
                    MIR_append_insn(ctx->context, ctx->func,
                                    MIR_new_insn(ctx->context, MIR_DMOV,
                                                 MIR_new_mem_op(ctx->context, MIR_T_D,
                                                                tSystem_Array->ManagedSize,
                                                                array_reg, index_reg, T->StackSize),
                                                 MIR_new_reg_op(ctx->context, value_reg)));
                } else {
                    CHECK_FAIL("TODO: memcpy array element");
                }
            } break;

            //----------------------------------------------------------------------------------------------------------
            // ceq - compare equal
            //----------------------------------------------------------------------------------------------------------
            case CEE_CEQ: {
                CHECK_AND_RETHROW(jit_compare(ctx, MIR_EQ));
            } break;

            default: {
                CHECK_FAIL("TODO: opcode %s", opcode_info->name);
            } break;
        }

    }

cleanup:
    if (ctx->func != NULL) {
        if (IS_ERROR(err)) {
            MIR_output_item(ctx->context, stdout, ctx->func);
        }
        MIR_finish_func(ctx->context);
        ctx->func = NULL;
    }

    // free the name of the method
    fclose(method_name);

    // free var names
    for (int i = 0; i < arrlen(var_names); i++) {
        fclose(var_names[i]);
        var_names[i] = NULL;
    }
    arrfree(var_names);

    // free the vars
    arrfree(vars);

    // free all the memory for this context
    for (int i = 0; i < hmlen(ctx->pc_to_stack_snapshot); i++) {
        arrfree(ctx->pc_to_stack_snapshot[i].stack.entries);
    }
    hmfree(ctx->pc_to_stack_snapshot);
    arrfree(ctx->stack.entries);

    return NO_ERROR;
}

#pragma clang diagnostic pop

err_t jit_assembly(System_Reflection_Assembly assembly) {
    err_t err = NO_ERROR;
    jit_context_t ctx = {};

    uint64_t start = microtime();

    // setup mir context
    ctx.context = MIR_init();
    MIR_module_t mod = MIR_new_module(ctx.context, "my_module");
    CHECK(mod != NULL);

    // setup special mir functions
    MIR_type_t res_type = MIR_T_P;
    ctx.gc_new_proto = MIR_new_proto(ctx.context, "gc_new$proto", 1, &res_type, 2, MIR_T_P, "type", MIR_T_U64, "size");
    ctx.gc_new_func = MIR_new_import(ctx.context, "gc_new");
    ctx.get_array_type_proto = MIR_new_proto(ctx.context, "get_array_type$proto", 1, &res_type, 1, MIR_T_P, "type");
    ctx.get_array_type_func = MIR_new_import(ctx.context, "get_array_type");

    // predefine all the types
    for (int i = 0; i < assembly->DefinedTypes->Length; i++) {
        System_Type type = assembly->DefinedTypes->Data[i];
        FILE* name = fcreate();
        print_full_type_name(type, name);
        fputc('\0', name);
        hmput(ctx.types, type, MIR_new_import(ctx.context, name->buffer));
        fclose(name);
    }

    // predefine all methods
    for (int i = 0; i < assembly->DefinedMethods->Length; i++) {
        System_Reflection_MethodInfo method = assembly->DefinedMethods->Data[i];
        CHECK_AND_RETHROW(prepare_method_signature(&ctx, method));
    }

    // now ir all the methods
    for (int ti = 0; ti < assembly->DefinedTypes->Length; ti++) {
        System_Type type = assembly->DefinedTypes->Data[ti];

        for (int mi = 0; mi < type->Methods->Length; mi++) {
            System_Reflection_MethodInfo method = type->Methods->Data[mi];
            CHECK_AND_RETHROW(jit_method(&ctx, method));
        }
    }

cleanup:
    if (ctx.context != NULL) {
        MIR_finish_module(ctx.context);

        uint64_t finish = microtime();
        MIR_output(ctx.context, stdout);

        TRACE("Took %dms to jit the binary", (finish - start) / 1000);
        MIR_finish(ctx.context);
    }

    hmfree(ctx.functions);
    hmfree(ctx.types);

    return err;
}
