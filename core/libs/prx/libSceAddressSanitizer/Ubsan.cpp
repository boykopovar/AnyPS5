#include "prx/libSceAddressSanitizer/include/Api.hpp"

#include <cstdlib>

namespace {

[[noreturn]] void Die() {
    std::abort();
}

const char* const noOptions = "";

}

extern "C" {

const char* APS5_VABI __ubsan_default_options() {
    return noOptions;
}

void APS5_VABI __ubsan_handle_add_overflow(const void*) {
}

void APS5_VABI __ubsan_handle_add_overflow_abort(const void*) {
    Die();
}

void APS5_VABI __ubsan_handle_alignment_assumption(const void*) {
}

void APS5_VABI __ubsan_handle_alignment_assumption_abort(const void*) {
    Die();
}

void APS5_VABI __ubsan_handle_builtin_unreachable(const void*) {
}

void APS5_VABI __ubsan_handle_divrem_overflow(const void*) {
}

void APS5_VABI __ubsan_handle_divrem_overflow_abort(const void*) {
    Die();
}

void APS5_VABI __ubsan_handle_dynamic_type_cache_miss(const void*) {
}

void APS5_VABI __ubsan_handle_dynamic_type_cache_miss_abort(const void*) {
    Die();
}

void APS5_VABI __ubsan_handle_float_cast_overflow(const void*) {
}

void APS5_VABI __ubsan_handle_float_cast_overflow_abort(const void*) {
    Die();
}

void APS5_VABI __ubsan_handle_function_type_mismatch(const void*) {
}

void APS5_VABI __ubsan_handle_function_type_mismatch_abort(const void*) {
    Die();
}

void APS5_VABI __ubsan_handle_function_type_mismatch_v1(const void*) {
}

void APS5_VABI __ubsan_handle_function_type_mismatch_v1_abort(const void*) {
    Die();
}

void APS5_VABI __ubsan_handle_implicit_conversion(const void*) {
}

void APS5_VABI __ubsan_handle_implicit_conversion_abort(const void*) {
    Die();
}

void APS5_VABI __ubsan_handle_invalid_builtin(const void*) {
}

void APS5_VABI __ubsan_handle_invalid_builtin_abort(const void*) {
    Die();
}

void APS5_VABI __ubsan_handle_load_invalid_value(const void*) {
}

void APS5_VABI __ubsan_handle_load_invalid_value_abort(const void*) {
    Die();
}

void APS5_VABI __ubsan_handle_missing_return(const void*) {
}

void APS5_VABI __ubsan_handle_mul_overflow(const void*) {
}

void APS5_VABI __ubsan_handle_mul_overflow_abort(const void*) {
    Die();
}

void APS5_VABI __ubsan_handle_negate_overflow(const void*) {
}

void APS5_VABI __ubsan_handle_negate_overflow_abort(const void*) {
    Die();
}

void APS5_VABI __ubsan_handle_nonnull_arg(const void*) {
}

void APS5_VABI __ubsan_handle_nonnull_arg_abort(const void*) {
    Die();
}

void APS5_VABI __ubsan_handle_nonnull_return(const void*) {
}

void APS5_VABI __ubsan_handle_nonnull_return_abort(const void*) {
    Die();
}

void APS5_VABI __ubsan_handle_nonnull_return_v1(const void*) {
}

void APS5_VABI __ubsan_handle_nonnull_return_v1_abort(const void*) {
    Die();
}

void APS5_VABI __ubsan_handle_nullability_arg(const void*) {
}

void APS5_VABI __ubsan_handle_nullability_arg_abort(const void*) {
    Die();
}

void APS5_VABI __ubsan_handle_nullability_return_v1(const void*) {
}

void APS5_VABI __ubsan_handle_nullability_return_v1_abort(const void*) {
    Die();
}

void APS5_VABI __ubsan_handle_out_of_bounds(const void*) {
}

void APS5_VABI __ubsan_handle_out_of_bounds_abort(const void*) {
    Die();
}

void APS5_VABI __ubsan_handle_pointer_overflow(const void*) {
}

void APS5_VABI __ubsan_handle_pointer_overflow_abort(const void*) {
    Die();
}

void APS5_VABI __ubsan_handle_shift_out_of_bounds(const void*) {
}

void APS5_VABI __ubsan_handle_shift_out_of_bounds_abort(const void*) {
    Die();
}

void APS5_VABI __ubsan_handle_sub_overflow(const void*) {
}

void APS5_VABI __ubsan_handle_sub_overflow_abort(const void*) {
    Die();
}

void APS5_VABI __ubsan_handle_type_mismatch(const void*) {
}

void APS5_VABI __ubsan_handle_type_mismatch_abort(const void*) {
    Die();
}

void APS5_VABI __ubsan_handle_type_mismatch_v1(const void*) {
}

void APS5_VABI __ubsan_handle_type_mismatch_v1_abort(const void*) {
    Die();
}

void APS5_VABI __ubsan_handle_vla_bound_not_positive(const void*) {
}

void APS5_VABI __ubsan_handle_vla_bound_not_positive_abort(const void*) {
    Die();
}

}
