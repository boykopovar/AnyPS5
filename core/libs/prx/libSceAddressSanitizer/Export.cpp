#include "prx/libSceAddressSanitizer/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI __asan_current_error_report() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_current_error_report_size() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_destroy_fake_stack() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_option_detect_stack_use_after_return() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_register_image_globals() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_unregister_image_globals() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __sanitizer_set_report_fd() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __ubsan_current_error_report() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __ubsan_current_error_report_size() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __ubsan_get_current_report_data() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __ubsan_handle_dynamic_type_cache_miss() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __ubsan_handle_dynamic_type_cache_miss_abort() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __ubsan_handle_function_type_mismatch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __ubsan_handle_function_type_mismatch_abort() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __ubsan_handle_function_type_mismatch_v1() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __ubsan_handle_function_type_mismatch_v1_abort() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __ubsan_handle_nonnull_return() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __ubsan_handle_nonnull_return_abort() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __ubsan_handle_type_mismatch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __ubsan_handle_type_mismatch_abort() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __ubsan_vptr_type_cache() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
