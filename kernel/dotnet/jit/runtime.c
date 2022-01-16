#include <proc/proc.h>
#include <dotnet/method_info.h>
#include <dotnet/gc/gc.h>
#include "runtime.h"

static THREAD_LOCAL stack_frame_t* m_stack_top;

void set_top_frame(stack_frame_t* frame) {
    if (frame->prev == NULL) {
        frame->prev = m_stack_top;
    }
    m_stack_top = frame;
}
