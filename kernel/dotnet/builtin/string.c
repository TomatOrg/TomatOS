#include <dotnet/gc/gc.h>
#include <dotnet/jit/runtime.h>
#include <dotnet/types.h>
#include <util/string.h>
#include <dotnet/method_info.h>
#include "string.h"

static struct method_info m_system_string_from_cstr = {
    .name = "system_string_from_cstr",
};

system_string_t* system_string_from_cstr(const char* data) {
    // setup the stack frame (ugly)
    stack_frame_t* stack_frame = __builtin_alloca(sizeof(stack_frame_t) + sizeof(void*));
    memset(stack_frame, 0, sizeof(stack_frame_t) + sizeof(void*));
    stack_frame->method_info = &m_system_string_from_cstr;
    stack_frame->objects_count = 1;
    set_top_frame(stack_frame);

    // actually allocate the object
    stack_frame->objects[0] = gc_alloc(g_string);
    system_string_t* string = stack_frame->objects[0];

    // initialize the object
    GC_WB(string, data, gc_alloc_array(g_char, strlen(data)));
    // TODO: initialize properly

    return stack_frame->objects[0];
}
