#include "internal_calls.h"
#include "dotnet/jit/jit.h"
#include "dotnet/gc/gc.h"
#include "mem/phys.h"
#include "mem/mem.h"

typedef struct System_Memory {
    System_Object Object;
    uint64_t Ptr;
    uint32_t Length;
} System_Memory;

static System_Exception Pentagon_MemoryServices_UpdateMemory(System_Memory* mem, System_Object holder, uint64_t ptr, uint32_t length) {
    gc_update_ref(&mem->Object, holder);
    mem->Ptr = ptr;
    mem->Length = length;
    return NULL;
}

static method_result_t Pentagon_MemoryServices_AllocateMemory(uint64_t size) {
    return (method_result_t){ .exception = NULL, .value = (uintptr_t)palloc(size) };
}

static System_Exception Pentagon_MemoryServices_FreeMemory(uint64_t ptr) {
    pfree((void *) ptr);
    return NULL;
}

static method_result_t Pentagon_MemoryServices_MapMemory(uint64_t phys, uint64_t pages) {
    vmm_map(phys, PHYS_TO_DIRECT(phys), pages, MAP_WRITE);
    return (method_result_t){ .exception = NULL, .value = (uintptr_t)PHYS_TO_DIRECT(phys) };
}

static void jit_MemoryServices_GetSpanPtr(MIR_context_t ctx) {
    const char* fname = "[Pentagon-v1]Pentagon.HAL.MemoryServices::GetSpanPtr([Corelib-v1]System.Span`1<uint8>&)";
    MIR_type_t res[] = {
        MIR_T_P,
        MIR_T_P
    };
    MIR_item_t func = MIR_new_func(ctx, fname, 2, res, 1, MIR_T_P, "this");
    MIR_reg_t this = MIR_reg(ctx, "this", func->u.func);
    MIR_append_insn(ctx, func,
                    MIR_new_ret_insn(ctx, 2,
                                     MIR_new_int_op(ctx, 0),
                                     MIR_new_mem_op(ctx, MIR_T_P, offsetof(System_Span, Ptr), this, 0, 1)));
    MIR_finish_func(ctx);
    MIR_new_export(ctx, fname);
}

err_t init_kernel_internal_calls() {
    err_t err = NO_ERROR;

    MIR_context_t ctx = jit_get_mir_context();

    MIR_load_external(ctx, "[Pentagon-v1]Pentagon.HAL.MemoryServices::UpdateMemory([Corelib-v1]System.Memory`1<uint8>&,object,uint64,int32)", Pentagon_MemoryServices_UpdateMemory);
    MIR_load_external(ctx, "[Pentagon-v1]Pentagon.HAL.MemoryServices::AllocateMemory(uint64)", Pentagon_MemoryServices_AllocateMemory);
    MIR_load_external(ctx, "[Pentagon-v1]Pentagon.HAL.MemoryServices::FreeMemory(uint64)", Pentagon_MemoryServices_FreeMemory);
    MIR_load_external(ctx, "[Pentagon-v1]Pentagon.HAL.MemoryServices::MapMemory(uint64,uint64)", Pentagon_MemoryServices_MapMemory);

    MIR_module_t pentagon = MIR_new_module(ctx, "pentagon");
    MIR_load_external(ctx, "[Pentagon-v1]Pentagon.HAL.MemoryServices::GetSpanPtr([Corelib-v1]System.Span`1<uint8>&)", Pentagon_MemoryServices_MapMemory);
    jit_MemoryServices_GetSpanPtr(ctx);
    MIR_finish_module(ctx);
    MIR_load_module(ctx, pentagon);


cleanup:
    jit_release_mir_context();

    return err;
}
