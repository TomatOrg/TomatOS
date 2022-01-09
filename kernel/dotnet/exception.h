#pragma once

#include <setjmp.h>

/**
 * The last thrown exception
 */
extern void* g_thrown_exception;

#define TRY \
    { \
        jmp_buf new_frame; \
        prev_frame = set_exception_frame(&new_frame); \
        if (setjmp(new_frame) == 0) { \
            if (1)

#define CATCH \
            else {} \
        } \
        set_exception_frame(prev_frame); \
    } \
    if (g_thrown_exception != NULL)

/**
 * Push an exception frame, will return the last exception that
 * was set.
 *
 * @param new       [IN] The new exception frame
 */
jmp_buf* set_exception_frame(jmp_buf* new);

/**
 * Ignore the exception, will un-protect the exception object from garbage collection
 */
void clear_exception();

/**
 * Throw an exception, this will protect the exception object from garbage collection
 *
 * @param ptr       [IN] The exception to throw
 */
void throw(void* ptr);
