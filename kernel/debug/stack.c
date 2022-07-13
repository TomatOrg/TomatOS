#include "stack.h"
#include "assert.h"
#include "thread/cpu_local.h"

#include <util/trace.h>

unsigned long __stack_chk_guard = 0xCAFEBABEDEADBEEF;

void __stack_chk_fail() {
    TRACE("stack protector failed at %d", get_cpu_id());
    ASSERT(!"stack protector failed!");
}
