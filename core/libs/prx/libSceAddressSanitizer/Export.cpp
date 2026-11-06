#include "prx/libSceAddressSanitizer/include/Api.hpp"
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

int APS5_VABI __asan_get_alloc_stack() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_get_free_stack() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI __asan_get_shadow_mapping(std::size_t*, std::size_t*) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI __asan_option_detect_stack_use_after_return() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_update_allocation_context() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __sanitizer_annotate_double_ended_contiguous_container() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __sanitizer_double_ended_contiguous_container_find_bad_address() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __sanitizer_get_current_allocated_bytes() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __sanitizer_get_free_bytes() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __sanitizer_get_heap_size() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __sanitizer_get_unmapped_bytes() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __sanitizer_verify_double_ended_contiguous_container() {
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

int APS5_VABI __ubsan_vptr_type_cache() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
