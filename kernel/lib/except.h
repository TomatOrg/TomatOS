#pragma once

#include <tomatodotnet/except.h>

#include "cpp_magic.h"
#include "defs.h"
#include "debug/log.h"

typedef enum err {
    NO_ERROR,

    /**
     * An unknown check failed
     */
    ERROR_CHECK_FAILED,

    /**
     * Ran out of memory trying to perform an action
     */
    ERROR_OUT_OF_MEMORY,

    /**
     * Unknown tdn error
     */
    ERROR_TDN_ERROR,

    /**
     * Got a uACPI error
     */
    ERROR_UACPI_ERROR,
} err_t;

/**
 * Check if there was an error
 */
#define IS_ERROR(err) (err != NO_ERROR)

//----------------------------------------------------------------------------------------------------------------------
// A check that fails if the expression returns false
//----------------------------------------------------------------------------------------------------------------------

const char* get_error_code(err_t err);
err_t map_tdn_error(tdn_err_t err);

#define CHECK_ERROR_LABEL(check, error, label, ...) \
    do { \
        if (UNLIKELY(!(check))) { \
            err = error; \
            IF(HAS_ARGS(__VA_ARGS__))(ERROR(__VA_ARGS__)); \
            ERROR("Check failed with error %s (%d) in function %s (%s:%d)", get_error_code(err), err, __FUNCTION__, __FILE__, __LINE__); \
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

#define RETHROW_LABEL(error, label) \
    do { \
        err = error; \
        if (UNLIKELY(IS_ERROR(err))) { \
            ERROR("\trethrown at %s (%s:%d)", __FUNCTION__, __FILE__, __LINE__); \
            goto label; \
        } \
    } while(0)

#define RETHROW(error) RETHROW_LABEL(error, cleanup)

#define TDN_RETHROW_LABEL(error, label) \
    do { \
        err = map_tdn_error(error); \
        if (UNLIKELY(IS_ERROR(err))) { \
            ERROR("\trethrown at %s (%s:%d)", __FUNCTION__, __FILE__, __LINE__); \
            goto label; \
        } \
    } while(0)

#define TDN_RETHROW(error) TDN_RETHROW_LABEL(error, cleanup)

//----------------------------------------------------------------------------------------------------------------------
// Assertion
//----------------------------------------------------------------------------------------------------------------------

#define ASSERT(expr) \
    do { \
        if (!(expr)) { \
            ERROR("Assertion failed at %s (%s:%d)", __FUNCTION__, __FILE__, __LINE__); \
            __builtin_trap(); \
        } \
    } while (0)
