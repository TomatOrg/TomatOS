#include "exception.h"

#include <stddef.h>
#include <setjmp.h>
#include <util/trace.h>
#include <util/except.h>

/**
 * The exception value, a managed object
 */
void* g_thrown_exception = NULL;

/**
 * The exception frame (not the stack frame)
 */
static jmp_buf* m_exception_frame;

jmp_buf* set_exception_frame(jmp_buf* new) {
    jmp_buf* last = m_exception_frame;
    m_exception_frame = new;
    return last;
}

void clear_exception() {
    g_thrown_exception = NULL;

    // TODO: Unprotect object
}

void throw(void* ptr) {
    // TODO: protect object
    g_thrown_exception = ptr;
    if (m_exception_frame != NULL) {
        longjmp(*m_exception_frame, 1);
    }

    // TODO: print exception nicely...
    ERROR("No exception handler to handle exception %p!", ptr);
    ASSERT(0);
}
