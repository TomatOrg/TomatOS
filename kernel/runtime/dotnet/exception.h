#pragma once

#define THROW(message) \
    do { \
        System_Exception __exception = GC_NEW(tSystem_Exception); \
        __exception->Message = new_string_from_cstr(message); \
        exception_throw(__exception); \
    } while (0)

void exception_throw(void* exception_obj);
