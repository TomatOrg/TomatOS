#include <util/except.h>
#include <util/string.h>

#include <stdbool.h>
#include <stdint.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Generic type handling
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define NO_SANITIZE __attribute__((no_sanitize("undefined")))

typedef struct source_location {
    const char* filename;
    uint32_t line;
    uint32_t column;
} source_location_t;

typedef struct type_descriptor {
    uint16_t kind;
#define TK_INTEGER  0x0000
#define TK_FLOAT    0x0001
#define TK_UNKNOWN  0xffff
    uint16_t info;
    char name[0];
} type_descriptor_t;

static NO_SANITIZE bool is_integer(const type_descriptor_t* type) {
    return type->kind == TK_INTEGER;
}

static NO_SANITIZE bool is_signed_integer(const type_descriptor_t* type) {
    return is_integer(type) && (type->info & 1);
}

static NO_SANITIZE bool is_unsigned_integer(const type_descriptor_t* type) {
    return is_integer(type) && !(type->info & 1);
}

static NO_SANITIZE unsigned get_integer_bit_width(const type_descriptor_t* type) {
    ASSERT(is_integer(type));
    return 1 << (type->info >> 1);
}

static NO_SANITIZE bool is_float(const type_descriptor_t* type) {
    return type->kind == TK_FLOAT;
}

static NO_SANITIZE unsigned get_float_bit_width(const type_descriptor_t* type) {
    ASSERT(is_float(type));
    return type->info;
}

static NO_SANITIZE bool is_inline_int(const type_descriptor_t* type) {
    ASSERT(is_integer(type));
    const unsigned inline_bits = sizeof(size_t) * 8;
    const unsigned bits = get_integer_bit_width(type);
    return bits <= inline_bits;
}

static NO_SANITIZE int64_t get_sint_value(const type_descriptor_t* type, size_t val) {
    ASSERT(is_signed_integer(type));
    if (is_inline_int(type)) {
        const unsigned extra_bits = sizeof(__int128_t) * 8 - get_integer_bit_width(type);
        return (__int128_t)(((__int128_t)val) << extra_bits) >> extra_bits;
    }

    if (get_integer_bit_width(type) == 64) {
        return *(int64_t*)(val);
    } else if (get_integer_bit_width(type) == 128) {
        ASSERT(!"ubsan without __int128 support");
    } else {
        ASSERT(!"unexpected bit width");
    }
}

static NO_SANITIZE uint64_t get_uint_value(const type_descriptor_t* type, size_t val) {
    ASSERT(is_unsigned_integer(type));
    if (is_inline_int(type)) {
        return val;
    }

    if (get_integer_bit_width(type) == 64) {
        return *(uint64_t*)(val);
    } else if (get_integer_bit_width(type) == 128) {
        ASSERT(!"ubsan without __int128 support");
    } else {
        ASSERT(!"unexpected bit width");
    }
}

static NO_SANITIZE uint64_t get_positive_int_value(const type_descriptor_t* type, size_t val) {
    if (is_unsigned_integer(type)) {
        return get_uint_value(type, val);
    }
    int64_t signed_val = get_sint_value(type, val);
    ASSERT(signed_val > 0);
    return signed_val;
}

static NO_SANITIZE bool is_minus_one(const type_descriptor_t* type, size_t val) {
    return is_signed_integer(type) && get_sint_value(type, val) == -1;
}

static NO_SANITIZE bool is_negative(const type_descriptor_t* type, size_t val) {
    return is_signed_integer(type) && get_sint_value(type, val) < 0;
}

static NO_SANITIZE void print_source_location(source_location_t data) {
    printf(" at %s:%d:%d\r\n", data.filename, data.line, data.column);
}

static NO_SANITIZE void print_value(const type_descriptor_t* type, size_t value) {
    if (is_signed_integer(type)) {
        printf("%lld", get_sint_value(type, value));
    } else if (is_unsigned_integer(type)) {
        printf("%llu", get_uint_value(type, value));
    } else {
        ASSERT(!"Unknown value");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Type mismatch
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct type_mismatch_data {
    source_location_t loc;
    const type_descriptor_t* type;
    unsigned char log_alignment;
    unsigned char type_check_kind;
} type_mismatch_data_t;

static const char* m_type_check_kinds[] = {
    "load of", "store to", "reference binding to", "member access within",
    "member call on", "constructor call on", "downcast of", "downcast of",
    "upcast of", "cast to virtual base of", "_Nonnull binding to",
    "dynamic operation on"
};

void NO_SANITIZE __ubsan_handle_type_mismatch_v1(type_mismatch_data_t* data, size_t pointer) {
    printf("[!] ubsan: ");

    size_t alignment = 1 << data->log_alignment;
    if (pointer == 0) {
        printf("%s null pointer of type %s",
               m_type_check_kinds[data->type_check_kind],
               data->type->name);
    } else if (pointer & (alignment - 1)) {
        printf("%s misaligned address %p for type %s, which requires %d byte alignment",
               m_type_check_kinds[data->type_check_kind],
               pointer,
               data->type->name,
               alignment);
    } else {
        printf("%s address %p with insufficient space for an object of type %s",
               m_type_check_kinds[data->type_check_kind],
                pointer,
                data->type->name);
    }

    print_source_location(data->loc);

    ASSERT_TRAP;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TODO: Alignment assumption
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct alignment_assumption_data {
    source_location_t loc;
    source_location_t assumption_loc;
    const type_descriptor_t* type;
} alignment_assumption_data_t;

void NO_SANITIZE __ubsan_handle_alignment_assumption(alignment_assumption_data_t* data, size_t pointer, size_t alignment, size_t offset) {
    size_t real_pointer = pointer - offset;
    size_t lsb = 0;
    size_t actual_alignment = 1ul << lsb;

    size_t mask = alignment - 1;
    size_t mis_alignment_offset = real_pointer & mask;

    printf("[!] ubsan: ");
    if (offset == 0) {
        printf("assumption of %d byte alignment for pointer of type %s failed",
               alignment, data->type->name);
    } else {
        printf("assumption of %d byte alignment (with offset of %d byte) for pointer of type %s failed",
               alignment, offset, data->type->name);
    }
    print_source_location(data->loc);

    printf("[!] ubsan: alignment assumption was specified");
    print_source_location(data->loc);

    WARN("ubsan: %saddress is %d aligned, misalignment offset is %d bytes",
         offset ? "offset " : "", actual_alignment, mis_alignment_offset);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Overflow
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct overflow_data {
    source_location_t loc;
    const type_descriptor_t* type;
} overflow_data_t;

#define UBSAN_HANDLE_OVERFLOW(opr) \
    printf("[!] ubsan: "); \
    bool is_signed = is_signed_integer(data->type); \
    printf("%s integer overflow: ", is_signed ? "signed" : "unsigned"); \
    print_value(data->type, lhs); \
    printf(" " opr " "); \
    print_value(data->type, rhs); \
    printf(" cannot be represented in type %s", data->type->name); \
    print_source_location(data->loc);

void NO_SANITIZE __ubsan_handle_add_overflow(overflow_data_t* data, size_t lhs, size_t rhs) { UBSAN_HANDLE_OVERFLOW("+"); }
void NO_SANITIZE __ubsan_handle_sub_overflow(overflow_data_t* data, size_t lhs, size_t rhs) { UBSAN_HANDLE_OVERFLOW("-"); }
void NO_SANITIZE __ubsan_handle_mul_overflow(overflow_data_t* data, size_t lhs, size_t rhs) { UBSAN_HANDLE_OVERFLOW("*"); }

void NO_SANITIZE __ubsan_handle_divrem_overflow(overflow_data_t* data, size_t lhs, size_t rhs) {
    ASSERT(!"TODO: __ubsan_handle_divrem_overflow");
}

void NO_SANITIZE __ubsan_handle_negate_overflow(overflow_data_t* data, size_t old_val) {
    ASSERT(!"TODO: __ubsan_handle_negate_overflow");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Shift out of bounds
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct shift_out_of_bounds {
    source_location_t loc;
    const type_descriptor_t* lhs_type;
    const type_descriptor_t* rhs_type;
} shift_out_of_bounds_t;

void NO_SANITIZE __ubsan_handle_shift_out_of_bounds(shift_out_of_bounds_t* data, size_t lhs, size_t rhs) {
    printf("[!] ubsan: ");

    if (
        is_negative(data->rhs_type, rhs) ||
        get_positive_int_value(data->rhs_type, rhs) >= get_integer_bit_width(data->lhs_type)
    ) {
        if (is_negative(data->rhs_type, rhs)) {
            printf("shift exponent ");
            print_value(data->rhs_type, rhs);
            printf(" is negative");
        } else {
            printf("shift exponent ");
            print_value(data->rhs_type, rhs);
            printf(" is too large for %d-bit type %s", get_integer_bit_width(data->lhs_type), data->lhs_type->name);
        }
    } else {
        if (is_negative(data->lhs_type, lhs)) {
            printf("left shift of negative value ");
            print_value(data->lhs_type, lhs);
        } else {
            printf("left shift of ");
            print_value(data->lhs_type, lhs);
            printf(" by ");
            print_value(data->rhs_type, rhs);
            printf(" places cannot be represented in type %s", data->lhs_type->name);
        }
    }

    print_source_location(data->loc);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Out of bounds
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct out_of_bounds_data {
    source_location_t loc;
    const type_descriptor_t* array_type;
    const type_descriptor_t* index_type;
} out_of_bounds_data_t;

void NO_SANITIZE __ubsan_handle_out_of_bounds(out_of_bounds_data_t* data, size_t index) {
    printf("[!] ubsan: ");
    printf("index %d out of bounds for type %s", index, data->array_type->name);
    print_source_location(data->loc);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Unreachable
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct unreachable_data {
    source_location_t loc;
} unreachable_data_t;

void NO_SANITIZE __ubsan_handle_builtin_unreachable(unreachable_data_t* data) {
    printf("[!] ubsan: ");
    printf("execution reached an unreachable program point");
    print_source_location(data->loc);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// VLA Bound
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct vla_bound_data {
    source_location_t loc;
    const type_descriptor_t* type;
} vla_bound_data_t;

void NO_SANITIZE __ubsan_handle_vla_bound_not_positive(vla_bound_data_t* data, size_t bound) {
    printf("[!] ubsan: ");
    printf("variable length array bound evaluates to non-positive value ");
    print_value(data->type, bound);
    print_source_location(data->loc);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Float Cast Overflow
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct float_cast_overflow {
    source_location_t loc;
    const type_descriptor_t* from_type;
    const type_descriptor_t* to_type;
} float_cast_overflow_t;

void NO_SANITIZE __ubsan_handle_float_cast_overflow(float_cast_overflow_t* data, size_t from) {
    printf("[!] ubsan: ");
    print_value(data->from_type, from);
    printf(" is outside of the range of representable values of type %s", data->to_type->name);
    print_source_location(data->loc);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Invalid Value
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct invalid_value_data {
    source_location_t loc;
    const type_descriptor_t* type;
} invalid_value_data_t;

void NO_SANITIZE __ubsan_handle_load_invalid_value(invalid_value_data_t* data, size_t val) {
    printf("[!] ubsan: ");

    printf("load of value ");
    print_value(data->type, val);
    printf(", which is not a valid value for type %s", data->type->name);

    print_source_location(data->loc);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TODO: Implicit Conversion
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Invalid builtin
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef enum builtin_check_kind {
    BCK_CTZ_PASSED_ZERO,
    BCK_CLZ_PASSED_ZERO,
} builtin_check_kind_t;

typedef struct invalid_builtin_data {
    source_location_t loc;
    unsigned char kind;
} invalid_builtin_data_t;

void NO_SANITIZE __ubsan_handle_invalid_builtin(invalid_builtin_data_t* data) {
    printf("[!] ubsan: ");
    printf("passing zero to %s, which is not a valid argument",
           (data->kind == BCK_CTZ_PASSED_ZERO) ? "ctz()" : "clz()");
    print_source_location(data->loc);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Pointer Overflow
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct pointer_overflow_data {
    source_location_t loc;
} pointer_overflow_data_t;

void NO_SANITIZE __ubsan_handle_pointer_overflow(pointer_overflow_data_t* data, size_t base, size_t result) {
    printf("[!] ubsan: ");

    if (base == 0 && result == 0) {
        printf("applying zero offset to null pointer");
    } else if (base == 0 && result != 0) {
        printf("applying non-zero offset %d to null pointer", result);
    } else if (base != 0 && result == 0) {
        printf("applying non-zero offset to non-null pointer %p produced null pointer", (void*)base);
    } else if (((intptr_t)base >= 0) == ((intptr_t)result >= 0)) {
        if (base > result) {
            printf("addition of unsigned offset to %p overflowed to %p", (void*)base, (void*)result);
        } else {
            printf("subtraction of unsigned offset to %p overflowed to %p", (void*)base, (void*)result);
        }
    } else {
        printf("pointer index expression with base %p overflowed to %p", (void*)base, (void*)result);
    }

    print_source_location(data->loc);
}
