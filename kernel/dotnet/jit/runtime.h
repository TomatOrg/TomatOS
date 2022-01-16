#pragma once

#include <dotnet/dotnet.h>

#define STACK_FRAME_PREV_OFFSET         (0)
#define STACK_FRAME_METHOD_INFO_OFFSET  (STACK_FRAME_PREV_OFFSET + 8)
#define STACK_FRAME_OBJECT_COUNT_OFFSET (STACK_FRAME_METHOD_INFO_OFFSET + 8)
#define STACK_FRAME_OBJECTS_OFFSET      (STACK_FRAME_OBJECT_COUNT_OFFSET + 2)

typedef struct stack_frame {
    struct stack_frame* prev;
    method_info_t method_info;
    uint16_t objects_count;
    void* objects[];
} stack_frame_t;

void set_top_frame(stack_frame_t* frame);

void* newobj(type_t type);
