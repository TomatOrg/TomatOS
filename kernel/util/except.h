#pragma once

#include "defs.h"
#include "trace.h"

typedef enum err {
    /**
      * There was no error, everything is good
      */
    NO_ERROR = 0,

    /**
     * Some check failed, basically an internal error
     */
    ERROR_CHECK_FAILED = 1,

    /**
     * The requested item was not found
     */
    ERROR_NOT_FOUND = 2,

    /**
     * The function ran out of resources to continue
     */
    ERROR_OUT_OF_MEMORY = 3,

    /**
     * Got a bad format, most likely when parsing file
     */
    ERROR_BAD_FORMAT = 4,

    /**
     * The runtime found an invalid opcode
     */
    ERROR_INVALID_OPCODE = 5,

    /**
     * The current thread does not own the lock for the specified object
     */
    ERROR_SYNCHRONIZATION_LOCK = 6,

    /**
     * Thrown when method invocation fails with an exception
     */
    ERROR_TARGET_INVOCATION = 7,

    /**
     * Could not find the wanted method
     */
    ERROR_MISSING_METHOD = 8,

    /**
     * Can not access the wanted member
     */
    ERROR_MEMBER_ACCESS = 9,
} err_t;

/**
 * Check if there was an error
 */
#define IS_ERROR(err) (err != NO_ERROR)

//----------------------------------------------------------------------------------------------------------------------
// Misc utilities
//----------------------------------------------------------------------------------------------------------------------

#define PANIC_ON(err) \
    do { \
        err_t ___err = err; \
        if (IS_ERROR(___err)) { \
           ERROR("Panic with error `%R` failed at %s (%s:%d)", ___err, __FUNCTION__, __FILE__, __LINE__); \
           while (1) __asm__("cli; hlt"); \
        } \
    } while(0)

_Noreturn void assertion_fail();
#define ASSERT(check) \
    do { \
        if (!(check)) { \
           ERROR("Assert `%s` failed at %s (%s:%d)", #check, __FUNCTION__, __FILE__, __LINE__); \
           assertion_fail(); \
        } \
    } while(0)

//----------------------------------------------------------------------------------------------------------------------
// A check that fails if the expression returns false
//----------------------------------------------------------------------------------------------------------------------

#if 1
#define ASSERT_ON_CHECK ASSERT(0)
#else
#define ASSERT_ON_CHECK
#endif

#define CHECK_ERROR_LABEL(check, error, label, ...) \
    do { \
        if (!(check)) { \
            err = error; \
            IF(HAS_ARGS(__VA_ARGS__))(ERROR(__VA_ARGS__)); \
            ERROR("Check failed with error %R in function %s (%s:%d)", err, __FUNCTION__, __FILE__, __LINE__); \
            ASSERT_ON_CHECK; \
            goto label; \
        } \
    } while(0)

#define CHECK_ERROR(check, error, ...)              CHECK_ERROR_LABEL(check, error, cleanup, ## __VA_ARGS__)
#define CHECK_LABEL(check, label, ...)              CHECK_ERROR_LABEL(check, ERROR_CHECK_FAILED, label, ## __VA_ARGS__)
#define CHECK(check, ...)                           CHECK_ERROR_LABEL(check, ERROR_CHECK_FAILED, cleanup, ## __VA_ARGS__)

#define DEBUG_CHECK_ERROR(check, error, ...)              CHECK_ERROR_LABEL(check, error, cleanup, ## __VA_ARGS__)
#define DEBUG_CHECK_LABEL(check, label, ...)              CHECK_ERROR_LABEL(check, ERROR_CHECK_FAILED, label, ## __VA_ARGS__)
#define DEBUG_CHECK(check, ...)                           CHECK_ERROR_LABEL(check, ERROR_CHECK_FAILED, cleanup, ## __VA_ARGS__)

//----------------------------------------------------------------------------------------------------------------------
// A check that fails without a condition
//----------------------------------------------------------------------------------------------------------------------

#define CHECK_FAIL(...)                             CHECK_ERROR_LABEL(0, ERROR_CHECK_FAILED, cleanup, ## __VA_ARGS__)
#define CHECK_FAIL_ERROR(error, ...)                CHECK_ERROR_LABEL(0, error, cleanup, ## __VA_ARGS__)
#define CHECK_FAIL_LABEL(label, ...)                CHECK_ERROR_LABEL(0, ERROR_CHECK_FAILED, label, ## __VA_ARGS__)
#define CHECK_FAIL_ERROR_LABEL(error, label, ...)   CHECK_ERROR_LABEL(0, error, label, ## __VA_ARGS__)

//----------------------------------------------------------------------------------------------------------------------
// A check that fails if an error was returned, used around functions returning an error
//----------------------------------------------------------------------------------------------------------------------

#define CHECK_AND_RETHROW_LABEL(error, label) \
    do { \
        err = error; \
        if (IS_ERROR(err)) { \
            ERROR("\trethrown at %s (%s:%d)", __FUNCTION__, __FILE__, __LINE__); \
            goto label; \
        } \
    } while(0)

#define CHECK_AND_RETHROW(error) CHECK_AND_RETHROW_LABEL(error, cleanup)
