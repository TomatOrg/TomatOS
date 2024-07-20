#include "except.h"

#include <tomatodotnet/except.h>

const char* get_error_code(err_t err) {
    switch (err) {
        case NO_ERROR: return "NO_ERROR";
        case ERROR_CHECK_FAILED: return "ERROR_CHECK_FAILED";
        case ERROR_OUT_OF_MEMORY: return "ERROR_OUT_OF_MEMORY";
        case ERROR_TDN_ERROR: return "<unknown TomatoDotNet error>";
        default: return "Unknown Error";
    }
}

err_t map_tdn_error(tdn_err_t err) {
    switch (err) {
        case TDN_NO_ERROR: return NO_ERROR;
        case TDN_ERROR_CHECK_FAILED: return ERROR_CHECK_FAILED;
        case TDN_ERROR_OUT_OF_MEMORY: return ERROR_OUT_OF_MEMORY;
        default: return ERROR_TDN_ERROR;
    }
}
