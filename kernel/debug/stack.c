#include "stack.h"
#include "assert.h"

#include <util/trace.h>

unsigned long __stack_chk_guard = 0xCAFEBABEDEADBEEF;

void __stack_chk_fail() {
    ASSERT(!"stack protector failed!");
}
