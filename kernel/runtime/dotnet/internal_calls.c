#include "internal_calls.h"
#include "dotnet/jit/jit.h"
#include "dotnet/gc/gc.h"
#include "mem/phys.h"
#include "mem/mem.h"
#include "dotnet/loader.h"
#include "acpi/acpi.h"
#include "kernel.h"
#include <irq/irq.h>
#include <arch/intrin.h>

// Uncomment this if you need to debug MamMemory-related stuff
#define MAPMEMORY_TRACE

typedef struct System_Memory {
    System_Object Object;
    uint64_t Ptr;
    uint32_t Length;
} System_Memory;

static System_Exception Pentagon_HAL_MemoryServices_UpdateMemory(System_Memory* mem, System_Object holder, uint64_t ptr, uint32_t length) {
    gc_update_ref(&mem->Object, holder);
    mem->Ptr = ptr;
    mem->Length = length;
    return NULL;
}

static method_result_t Pentagon_HAL_MemoryServices_AllocateMemory(uint64_t size) {
    return (method_result_t){ .exception = NULL, .value = (uintptr_t)palloc(size) };
}

static System_Exception Pentagon_HAL_MemoryServices_FreeMemory(uint64_t ptr) {
    pfree((void *) ptr);
    return NULL;
}

static method_result_t Pentagon_HAL_MemoryServices_MapMemory(uint64_t phys, uint64_t pages) {
#ifdef MAPMEMORY_TRACE
    printf("Pentagon.DriverServices.MemoryServices::MapMemory(0x%p, %d)\n", phys, pages);
#endif
    vmm_map(phys, PHYS_TO_DIRECT(phys), pages, MAP_WRITE);
    return (method_result_t){ .exception = NULL, .value = (uintptr_t)PHYS_TO_DIRECT(phys) };
}

static void jit_MemoryServices_GetSpanPtr(MIR_context_t ctx) {
    const char* fname = "uint64 [Pentagon-v1]Pentagon.DriverServices.MemoryServices::GetSpanPtr([Corelib-v1]System.Span`1<uint8>&)";
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

static err_t pentagon_gen(MIR_context_t ctx, System_Reflection_MethodInfo method) {
    err_t err = NO_ERROR;

    if (method->GenericMethodDefinition != NULL) {
        if (string_equals_cstr( method->GenericMethodDefinition->Name, "UnsafePtrToRef")) {
            MIR_reg_t this = MIR_reg(ctx, "arg0", method->MirFunc->u.func);
            MIR_append_insn(ctx, method->MirFunc,
                            MIR_new_ret_insn(ctx, 2,
                                             MIR_new_int_op(ctx, 0),
                                             MIR_new_reg_op(ctx, this)));
        } else {
            CHECK_FAIL("%U", method->Name);
        }
    } else {
        CHECK_FAIL("%U", method->Name);
    }

cleanup:
    return err;
}

static bool pentagon_can_gen(System_Reflection_MethodInfo method) {
    System_Type type = method->DeclaringType;
    if (string_equals_cstr(type->Namespace, "Pentagon.DriverServices")) {
        if (string_equals_cstr(type->Name, "MemoryServices")) {
            if (method->GenericMethodDefinition != NULL) {
                method = method->GenericMethodDefinition;
                if (string_equals_cstr(method->Name, "UnsafePtrToRef")) return true;
            }
        }
    }
    return false;
}

static jit_generic_extern_hook_t m_jit_extern_hook = {
    .can_gen = pentagon_can_gen,
    .gen = pentagon_gen,
};

static System_Exception Pentagon_DriverServices_Log_LogHex(uint64_t val) {
    printf("%02x", val);
    return NULL;
}

static System_Exception Pentagon_DriverServices_Log_LogString(System_String val) {
    printf("%U", val);
    return NULL;
}

static method_result_t Pentagon_DriverServices_Acpi_GetRsdt() {
    return (method_result_t){ .exception = NULL, .value = DIRECT_TO_PHYS(m_rsdt) };
}

static method_result_t Pentagon_GetMappedPhysicalAddress(System_Memory memory) {
    return (method_result_t){ .exception = NULL, .value = DIRECT_TO_PHYS(memory.Ptr) };
}

// `ctx` is the vector mask address
void msix_irq_mask(void *ctx) {
    *(uint32_t*)(PHYS_TO_DIRECT(ctx)) = 1;
}

void msix_irq_unmask(void *ctx) {
    *(uint32_t*)(PHYS_TO_DIRECT(ctx)) = 0;
}

irq_ops_t m_msix_irq_ops = {
    .mask = msix_irq_mask,
    .unmask = msix_irq_unmask
};

// `ctx` is the page aligned IOAPIC address ORed with the index in the IoRedTbl
// FIXME: this code does one VMEXIT more than it needs to, since it reads the flags value

void ioapic_irq_mask(void *ctx) {
    uintptr_t c = (uintptr_t)ctx;
    uint32_t index = c & 4095;
    volatile uint32_t *sel = PHYS_TO_DIRECT(c - index), *win = sel + 4;
    *sel = 0x10 + index * 2;
    *win |= 1u << 16;
}

void ioapic_irq_unmask(void *ctx) {
    uintptr_t c = (uintptr_t)ctx;
    uint32_t index = c & 4095;
    volatile uint32_t *sel = PHYS_TO_DIRECT(c - index), *win = sel + 4;
    *sel = 0x10 + index * 2;
    *win &= ~(1u << 16);
}

irq_ops_t m_ioapic_irq_ops = {
    .mask = ioapic_irq_mask,
    .unmask = ioapic_irq_unmask
};

static method_result_t Pentagon_AllocateIrq(int count, int type, void* addr) {
    uint8_t interrupt = 0;
    irq_ops_t ops = {};
    switch (type) {
    case 0: // MSIX
        ops = m_msix_irq_ops;
        break;
    case 2: // IOAPIC
        ops = m_ioapic_irq_ops;
        break;
    default:
        ASSERT(!"IRQ type not supported yet");
    }
    alloc_irq(count, ops, addr, &interrupt);
    return (method_result_t){ .exception = NULL, .value = interrupt };
}

static method_result_t Pentagon_GetNextFramebuffer(int* index, uint64_t* addr, int* width, int* height, int* pitch) {
    while (true) {
        if (*index >= g_framebuffers_count) {
            return (method_result_t){ .exception = NULL, .value = false };
        }

        // get it and increment
        struct limine_framebuffer* framebuffer = &g_framebuffers[*index];
        (*index)++;

        // check it is valid for us
        if (framebuffer->bpp != 32) continue;
        if (framebuffer->red_mask_size != 8) continue;
        if (framebuffer->green_mask_size != 8) continue;
        if (framebuffer->blue_mask_size != 8) continue;
        if (framebuffer->red_mask_shift != 16) continue;
        if (framebuffer->green_mask_shift != 8) continue;
        if (framebuffer->blue_mask_shift != 0) continue;
        if (framebuffer->memory_model != LIMINE_FRAMEBUFFER_RGB) continue;

        // move it out
        *addr = (uint64_t)DIRECT_TO_PHYS(framebuffer->address);
        *width = (int)framebuffer->width;
        *height = (int)framebuffer->height;
        *pitch = (int)framebuffer->pitch;

        // return we found one
        return (method_result_t){ .exception = NULL, .value = true };
    }
}

static System_Exception Pentagon_GetDefaultFont(uint64_t* addr, int* size) {
    *addr = DIRECT_TO_PHYS(g_default_font.address);
    *size = (int)g_default_font.size;
    return NULL;
}

static System_Exception Pentagon_IrqWait(uint64_t irq) {
    irq_wait(irq);
    return NULL;
}


static method_result_t Pentagon_DriverServices_IoPorts_In8(uint16_t port) {
    return (method_result_t){ .exception = NULL, .value = __inbyte(port) };
}

static System_Exception Pentagon_DriverServices_IoPorts_Out8(uint16_t port, uint8_t value) {
    __outbyte(port, value);
    return NULL;
}


err_t init_kernel_internal_calls() {
    err_t err = NO_ERROR;

    jit_add_extern_whitelist("Pentagon.dll");
    jit_add_generic_extern_hook( &m_jit_extern_hook);

    MIR_context_t ctx = jit_get_mir_context();

    // TODO: rename the functions so they will match nicely
    MIR_load_external(ctx, "[Pentagon-v1]Pentagon.DriverServices.MemoryServices::UpdateMemory([Corelib-v1]System.Memory`1<uint8>&,object,uint64,int32)", Pentagon_HAL_MemoryServices_UpdateMemory);
    MIR_load_external(ctx, "uint64 [Pentagon-v1]Pentagon.DriverServices.MemoryServices::AllocateMemory(uint64)", Pentagon_HAL_MemoryServices_AllocateMemory);
    MIR_load_external(ctx, "[Pentagon-v1]Pentagon.DriverServices.MemoryServices::FreeMemory(uint64)", Pentagon_HAL_MemoryServices_FreeMemory);
    MIR_load_external(ctx, "uint64 [Pentagon-v1]Pentagon.DriverServices.MemoryServices::MapMemory(uint64,uint64)", Pentagon_HAL_MemoryServices_MapMemory);
    MIR_load_external(ctx, "uint64 [Pentagon-v1]Pentagon.DriverServices.MemoryServices::GetMappedPhysicalAddress([Corelib-v1]System.Memory`1<uint8>)", Pentagon_GetMappedPhysicalAddress);

    MIR_load_external(ctx, "[Pentagon-v1]Pentagon.DriverServices.Log::LogHex(uint64)", Pentagon_DriverServices_Log_LogHex);
    MIR_load_external(ctx, "[Pentagon-v1]Pentagon.DriverServices.Log::LogString(string)", Pentagon_DriverServices_Log_LogString);

    MIR_load_external(ctx, "int32 [Pentagon-v1]Pentagon.DriverServices.Irq::AllocateIrq(int32,[Pentagon-v1]Pentagon.DriverServices.Irq+IrqMaskType,uint64)", Pentagon_AllocateIrq);
    MIR_load_external(ctx, "[Pentagon-v1]Pentagon.DriverServices.Irq::IrqWait(int32)", Pentagon_IrqWait);

    MIR_load_external(ctx, "uint64 [Pentagon-v1]Pentagon.DriverServices.Acpi.Acpi::GetRsdt()", Pentagon_DriverServices_Acpi_GetRsdt);

    MIR_load_external(ctx, "uint8 [Pentagon-v1]Pentagon.DriverServices.IoPorts::In8(uint16)", Pentagon_DriverServices_IoPorts_In8);
    MIR_load_external(ctx, "[Pentagon-v1]Pentagon.DriverServices.IoPorts::Out8(uint16,uint8)", Pentagon_DriverServices_IoPorts_Out8);

    MIR_load_external(ctx, "bool [Pentagon-v1]Pentagon.DriverServices.KernelUtils::GetNextFramebuffer([Corelib-v1]System.Int32&,[Corelib-v1]System.UInt64&,[Corelib-v1]System.Int32&,[Corelib-v1]System.Int32&,[Corelib-v1]System.Int32&)", Pentagon_GetNextFramebuffer);
    MIR_load_external(ctx, "[Pentagon-v1]Pentagon.DriverServices.KernelUtils::GetDefaultFont([Corelib-v1]System.UInt64&,[Corelib-v1]System.Int32&)", Pentagon_GetDefaultFont);

    MIR_module_t pentagon = MIR_new_module(ctx, "pentagon");
    jit_MemoryServices_GetSpanPtr(ctx);
    MIR_finish_module(ctx);
    MIR_load_module(ctx, pentagon);


cleanup:
    jit_release_mir_context();

    return err;
}
