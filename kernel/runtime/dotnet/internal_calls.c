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

err_t init_kernel_internal_calls() {
    err_t err = NO_ERROR;

    MIR_context_t ctx = jit_get_mir_context();

    MIR_load_external(ctx, "[Pentagon-v1]Pentagon.MemoryServices::UpdateMemory([Corelib-v1]System.Memory`1<uint8>&,object,uint64,int32)", Pentagon_MemoryServices_UpdateMemory);
    MIR_load_external(ctx, "[Pentagon-v1]Pentagon.MemoryServices::AllocateMemory(uint64)", Pentagon_MemoryServices_AllocateMemory);
    MIR_load_external(ctx, "[Pentagon-v1]Pentagon.MemoryServices::FreeMemory(uint64)", Pentagon_MemoryServices_FreeMemory);
    MIR_load_external(ctx, "[Pentagon-v1]Pentagon.MemoryServices::MapMemory(uint64,uint64)", Pentagon_MemoryServices_MapMemory);

cleanup:
    jit_release_mir_context();

    return err;
}
