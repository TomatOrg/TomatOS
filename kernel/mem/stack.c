#include "stack.h"

#include <lib/defs.h>
#include <lib/list.h>
#include <sync/spinlock.h>

#include "memory.h"

static _Atomic(uintptr_t) m_small_stack_watermark = SMALL_STACKS_ADDR;

void* small_stack_alloc(void) {
    // just add to the bump
    uintptr_t bottom = atomic_fetch_add_explicit(&m_small_stack_watermark, SIZE_32KB, memory_order_relaxed);
    m_small_stack_watermark += SIZE_32KB;

    // TODO: fail when too much

    // return the top of the stack
    return (void*)(bottom + SIZE_32KB);
}

