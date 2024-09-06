#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <lib/except.h>

#include "lib/string.h"

#include "log.h"

typedef struct source_location {
    const char* filename;
    uint32_t line;
    uint32_t column;
} source_location_t;

typedef enum type_descriptor_kind {
    TK_INTEGER = 0x0000,
    TK_FLOAT = 0x0001,
    TK_UNKNOWN = 0xffff,
} type_descriptor_kind_t;

typedef struct type_descriptor {
    uint16_t kind;
    uint16_t type_info;
    char type_name[1];
} type_descriptor_t;

static bool is_integer_ty(const type_descriptor_t* type) {
    return type->kind == TK_INTEGER;
}

static bool is_signed_integer_ty(const type_descriptor_t* type) {
    return is_integer_ty(type) && (type->type_info & 1);
}

static bool is_unsigned_integer_ty(const type_descriptor_t* type) {
    return is_integer_ty(type) && !(type->type_info & 1);
}

static unsigned get_integer_bit_width(const type_descriptor_t* type) {
    ASSERT(is_integer_ty(type));
    return 1 << (type->type_info >> 1);
}

static bool is_minus_one(size_t value, const type_descriptor_t* type) {
    return is_signed_integer_ty(type) && ((intptr_t)value == -1);
}

static bool is_negative(size_t value, const type_descriptor_t* type) {
    return is_signed_integer_ty(type) && ((intptr_t)value < 0);
}

static bool is_inline_int(const type_descriptor_t* type) {
    ASSERT(is_integer_ty(type));
    const unsigned inline_bits = sizeof(size_t) * 8;
    const unsigned bits = get_integer_bit_width(type);
    return bits <= inline_bits;
}

static intptr_t get_sint_value(size_t value, const type_descriptor_t* type) {
    ASSERT(is_signed_integer_ty(type));
    if (is_inline_int(type)) {
        const unsigned extra_bits = sizeof(intptr_t) * 8 - get_integer_bit_width(type);
        return (intptr_t)(value << extra_bits) >> extra_bits;
    }
    ASSERT(!"unexpected bit width");
}

static uintptr_t get_uint_value(size_t value, const type_descriptor_t* type) {
    ASSERT(is_unsigned_integer_ty(type));
    if (is_inline_int(type)) {
        return value;
    }
    ASSERT(!"unexpected bit width");
}

static uintptr_t get_positive_int_value(size_t value, const type_descriptor_t* type) {
    if (is_unsigned_integer_ty(type)) {
        return get_uint_value(value, type);
    }
    intptr_t val = get_sint_value(value, type);
    ASSERT(val >= 0);
    return val;
}

typedef struct overflow_data {
    source_location_t loc;
    const type_descriptor_t* type;
} overflow_data_t;

#define LOG_UBSAN(fmt, ...) LOG_WARN("ubsan: " fmt " at %s:%d:%d", ## __VA_ARGS__, data->loc.filename, data->loc.line, data->loc.column);

static void handle_integer_overflow(overflow_data_t* data, size_t lhs, const char* operator, size_t rhs) {
    bool is_signed = is_signed_integer_ty(data->type);

    LOG_UBSAN("%s integer overflow: "
        "%lld %s %llx cannot be represented in type %s",
        is_signed ? "signed" : "unsigned", lhs, operator, rhs, data->type->type_name);
}

void __ubsan_handle_add_overflow(overflow_data_t* data, size_t lhs, size_t rhs) { handle_integer_overflow(data, lhs, "+", rhs); }
void __ubsan_handle_sub_overflow(overflow_data_t* data, size_t lhs, size_t rhs) { handle_integer_overflow(data, lhs, "-", rhs); }
void __ubsan_handle_mul_overflow(overflow_data_t* data, size_t lhs, size_t rhs) { handle_integer_overflow(data, lhs, "*", rhs); }

void __ubsan_handle_negate_overflow(overflow_data_t* data, size_t old_val) {
    bool is_signed = is_signed_integer_ty(data->type);

    if (is_signed) {
        LOG_UBSAN("negation of %lld cannot be represented in type %s; "
            "cast to an unsigned type to negate this value to itself",
            old_val, data->type->type_name);
    } else {
        LOG_UBSAN("negation of %llu cannot be represented in type %s",
            old_val, data->type->type_name);
    }
}

void __ubsan_handle_divrem_overflow(overflow_data_t* data, size_t lhs, size_t rhs) {
    if (is_minus_one(rhs, data->type)) {
        LOG_UBSAN("division of %lld by -1 cannot be represented in type %s", data->type->type_name);
    } else {
        LOG_UBSAN("division by zero");
    }
}

typedef struct shift_out_of_bounds_data {
    source_location_t loc;
    const type_descriptor_t* lhs_type;
    const type_descriptor_t* rhs_type;
} shift_out_of_bounds_data_t;

void __ubsan_handle_shift_out_of_bounds(shift_out_of_bounds_data_t* data, size_t lhs, size_t rhs) {
    if (is_negative(rhs, data->rhs_type)) {
        LOG_UBSAN("shift exponent %lld is negative", rhs);
    } else if (get_positive_int_value(rhs, data->rhs_type) >= get_integer_bit_width(data->lhs_type)) {
        LOG_UBSAN("shift exponent %lld is too large for %d-bit type %s",
            rhs, get_integer_bit_width(data->lhs_type), data->lhs_type->type_name);
    } else if (is_negative(lhs, data->lhs_type)) {
        LOG_UBSAN("left shift of negative value %lld", lhs);
    } else {
        LOG_UBSAN("left shift of %llu by %llu places cannot be represented in type %s",
            lhs, rhs, data->lhs_type->type_name);
    }
}

typedef struct out_of_bounds_data {
    source_location_t loc;
    const type_descriptor_t* array_type;
    const type_descriptor_t* index_type;
} out_of_bounds_data_t;

void __ubsan_handle_out_of_bounds(out_of_bounds_data_t* data, size_t index) {
    LOG_UBSAN("index %lld out of bounds of type %s", index, data->array_type->type_name);
}

typedef struct pointer_overflow_data {
    source_location_t loc;
} pointer_overflow_data_t;

void __ubsan_handle_pointer_overflow(pointer_overflow_data_t* data, size_t base, size_t result) {
    if (base == 0 && result == 0) {
        LOG_UBSAN("applying zero offset to null pointer");
    } else if (base == 0 && result != 0) {
        LOG_UBSAN("applying non-zero offset %lld to null pointer", result);
    } else if (base != 0 && result == 0) {
        LOG_UBSAN("applying non-zero offset to non-null pointer %p produced null pointer", base);
    } else if (((intptr_t)base >= 0) == ((intptr_t)result >= 0)) {
        if (base > result) {
            LOG_UBSAN("addition of unsigned offset to %p overflowed to %p", base, result);
        } else {
            LOG_UBSAN("subtraction of unsigned offset to %p overflowed to %p", base, result);
        }
    } else {
        LOG_UBSAN("pointer index expression with base %p overflowed to %p", base, result);
    }
}

typedef struct invalid_value_data {
    source_location_t loc;
    const type_descriptor_t* type;
} invalid_value_data_t;

void __ubsan_handle_load_invalid_value(invalid_value_data_t* data, size_t val) {
    LOG_UBSAN("load of value %lld, which is not a valid value for type %s", val, data->type->type_name);
}

typedef enum builtin_check_kind {
    BCK_CTZ_PASSED_ZERO,
    BCK_CLZ_PASSED_ZERO,
} builtin_check_kind_t;

typedef struct invalid_builtin_data {
    source_location_t loc;
    unsigned char kind;
} invalid_builtin_data_t;

void __ubsan_handle_invalid_builtin(invalid_builtin_data_t* data) {
    LOG_UBSAN("passing zero to %s, which is not a valid argument", (data->kind == BCK_CTZ_PASSED_ZERO) ? "ctz()" : "clz()");
}

typedef struct function_type_mismatch_data {
    source_location_t loc;
    const type_descriptor_t* type;
} function_type_mismatch_data_t;

void __ubsan_handle_function_type_mismatch(function_type_mismatch_data_t* data, size_t function) {
    LOG_UBSAN("call to function %p through pointer to incorrect function type %s", function, data->type->type_name);
}

typedef struct type_mismatch_data {
    source_location_t loc;
    const type_descriptor_t* type;
    unsigned char log_alignment;
    unsigned char type_check_kind;
} type_mismatch_data_t;

const char* const m_type_check_kinds[] = {
    "load of", "store to", "reference binding to", "member access within",
    "member call on", "constructor call on", "downcast of", "downcast of",
    "upcast of", "cast to virtual base of", "_Nonnull binding to",
    "dynamic operation on"
};

void __ubsan_handle_type_mismatch_v1(type_mismatch_data_t* data, size_t pointer) {
    size_t alignment = (size_t)1 << data->log_alignment;

    if (pointer == 0) {
        LOG_UBSAN("%s null pointer of type %s", m_type_check_kinds[data->type_check_kind], data->type->type_name);
    } else if (pointer & (alignment - 1)) {
        LOG_UBSAN("%s misaligned address %p for type %s, "
            "which requires %d byte alignment",
            m_type_check_kinds[data->type_check_kind], pointer, alignment, data->type->type_name);
    } else {
        LOG_UBSAN("%s address %p with insufficient space "
            "for an object of type %s",
            m_type_check_kinds[data->type_check_kind], pointer, data->type->type_name);
    }
}
