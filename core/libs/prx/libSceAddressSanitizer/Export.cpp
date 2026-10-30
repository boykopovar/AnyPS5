#include "prx/libSceAddressSanitizer/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "asan_anyps5/Interface.hpp"

extern "C" {

int __asan_option_detect_stack_use_after_return = 0;
std::uintptr_t __ubsan_vptr_type_cache[128] = {};

void* APS5_VABI __asan_addr_is_in_fake_stack(void* fakeStack, void* address, void** begin, void** end) {
    return AsanRuntime::Impl::__asan_addr_is_in_fake_stack_nid_no_patch(fakeStack, address, begin, end, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

int APS5_VABI __asan_address_is_poisoned(const volatile void* address) {
    return AsanRuntime::Impl::__asan_address_is_poisoned_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_after_dynamic_init() {
    return;
}

void* APS5_VABI __asan_aligned_alloc(std::size_t alignment, std::size_t size) {
    return AsanRuntime::Impl::__asan_aligned_alloc_nid_no_patch(alignment, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_alloca_poison(std::uintptr_t addr, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_alloca_poison_nid_no_patch(addr, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_allocas_unpoison(std::uintptr_t top, std::uintptr_t bottom) {
    return AsanRuntime::Impl::__asan_allocas_unpoison_nid_no_patch(top, bottom, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_before_dynamic_init(const char* moduleName) {
    return AsanRuntime::Impl::__asan_before_dynamic_init_nid_no_patch(moduleName, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void* APS5_VABI __asan_calloc(std::size_t count, std::size_t size) {
    return AsanRuntime::Impl::__asan_calloc_nid_no_patch(count, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

int APS5_VABI __asan_current_error_report() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_current_error_report_size() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI __asan_describe_address(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_describe_address_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

int APS5_VABI __asan_destroy_fake_stack() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI __asan_exp_load1(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_exp_load1_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_exp_load16(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_exp_load16_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_exp_load16_noabort(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_exp_load16_noabort_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_exp_load1_noabort(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_exp_load1_noabort_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_exp_load2(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_exp_load2_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_exp_load2_noabort(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_exp_load2_noabort_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_exp_load4(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_exp_load4_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_exp_load4_noabort(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_exp_load4_noabort_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_exp_load8(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_exp_load8_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_exp_load8_noabort(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_exp_load8_noabort_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_exp_loadN(std::uintptr_t address, std::uintptr_t size, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_exp_loadN_nid_no_patch(address, size, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_exp_loadN_noabort(std::uintptr_t address, std::uintptr_t size, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_exp_loadN_noabort_nid_no_patch(address, size, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_exp_store1(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_exp_store1_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_exp_store16(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_exp_store16_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_exp_store16_noabort(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_exp_store16_noabort_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_exp_store1_noabort(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_exp_store1_noabort_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_exp_store2(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_exp_store2_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_exp_store2_noabort(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_exp_store2_noabort_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_exp_store4(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_exp_store4_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_exp_store4_noabort(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_exp_store4_noabort_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_exp_store8(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_exp_store8_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_exp_store8_noabort(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_exp_store8_noabort_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_exp_storeN(std::uintptr_t address, std::uintptr_t size, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_exp_storeN_nid_no_patch(address, size, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_exp_storeN_noabort(std::uintptr_t address, std::uintptr_t size, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_exp_storeN_noabort_nid_no_patch(address, size, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_free(void* pointer) {
    return AsanRuntime::Impl::__asan_free_nid_no_patch(pointer, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uintptr_t APS5_VABI __asan_get_alloc_stack(std::uintptr_t address, std::uintptr_t* trace, std::uintptr_t size, std::uint32_t* threadId) {
    return AsanRuntime::Impl::__asan_get_alloc_stack_nid_no_patch(address, trace, size, threadId, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void* APS5_VABI __asan_get_current_fake_stack() {
    return AsanRuntime::Impl::__asan_get_current_fake_stack_nid_no_patch({reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uintptr_t APS5_VABI __asan_get_free_stack(std::uintptr_t address, std::uintptr_t* trace, std::uintptr_t size, std::uint32_t* threadId) {
    return AsanRuntime::Impl::__asan_get_free_stack_nid_no_patch(address, trace, size, threadId, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::size_t APS5_VABI __asan_get_report_access_size() {
    return AsanRuntime::Impl::__asan_get_report_access_size_nid_no_patch({reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

int APS5_VABI __asan_get_report_access_type() {
    return AsanRuntime::Impl::__asan_get_report_access_type_nid_no_patch({reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void* APS5_VABI __asan_get_report_address() {
    return AsanRuntime::Impl::__asan_get_report_address_nid_no_patch({reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void* APS5_VABI __asan_get_report_bp() {
    return AsanRuntime::Impl::__asan_get_report_bp_nid_no_patch({reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

const char* APS5_VABI __asan_get_report_description() {
    return AsanRuntime::Impl::__asan_get_report_description_nid_no_patch({reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void* APS5_VABI __asan_get_report_pc() {
    return AsanRuntime::Impl::__asan_get_report_pc_nid_no_patch({reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void* APS5_VABI __asan_get_report_sp() {
    return AsanRuntime::Impl::__asan_get_report_sp_nid_no_patch({reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_get_shadow_mapping(std::size_t* scale, std::size_t* offset) {
    return AsanRuntime::Impl::__asan_get_shadow_mapping_nid_no_patch(scale, offset, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_handle_no_return() {
    return AsanRuntime::Impl::__asan_handle_no_return_nid_no_patch({reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_init() {
    return AsanRuntime::Impl::__asan_init_nid_no_patch({reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

int APS5_VABI __asan_init_v5() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI __asan_load1(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_load1_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_load16(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_load16_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_load16_noabort(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_load16_noabort_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_load1_noabort(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_load1_noabort_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_load2(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_load2_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_load2_noabort(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_load2_noabort_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_load4(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_load4_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_load4_noabort(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_load4_noabort_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_load8(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_load8_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_load8_noabort(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_load8_noabort_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_loadN(std::uintptr_t address, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_loadN_nid_no_patch(address, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_loadN_noabort(std::uintptr_t address, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_loadN_noabort_nid_no_patch(address, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uintptr_t APS5_VABI __asan_load_cxx_array_cookie(std::uintptr_t* p) {
    return AsanRuntime::Impl::__asan_load_cxx_array_cookie_nid_no_patch(p, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

const char* APS5_VABI __asan_locate_address(std::uintptr_t address, char* name, std::uintptr_t nameSize, std::uintptr_t* regionAddress, std::uintptr_t* regionSize) {
    return AsanRuntime::Impl::__asan_locate_address_nid_no_patch(address, name, nameSize, regionAddress, regionSize, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void* APS5_VABI __asan_malloc(std::size_t size) {
    return AsanRuntime::Impl::__asan_malloc_nid_no_patch(size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

int APS5_VABI __asan_malloc_stats() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::size_t APS5_VABI __asan_malloc_usable_size(const void* pointer) {
    return AsanRuntime::Impl::__asan_malloc_usable_size_nid_no_patch(pointer, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void* APS5_VABI __asan_memalign(std::size_t alignment, std::size_t size) {
    return AsanRuntime::Impl::__asan_memalign_nid_no_patch(alignment, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void* APS5_VABI __asan_memcpy(void* to, const void* from, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_memcpy_nid_no_patch(to, from, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void* APS5_VABI __asan_memmove(void* to, const void* from, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_memmove_nid_no_patch(to, from, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void* APS5_VABI __asan_memset(void* block, int value, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_memset_nid_no_patch(block, value, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_poison_cxx_array_cookie(std::uintptr_t p) {
    return AsanRuntime::Impl::__asan_poison_cxx_array_cookie_nid_no_patch(p, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_poison_intra_object_redzone(std::uintptr_t address, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_poison_intra_object_redzone_nid_no_patch(address, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_poison_memory_region(const volatile void* addr, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_poison_memory_region_nid_no_patch(addr, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_poison_stack_memory(std::uintptr_t address, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_poison_stack_memory_nid_no_patch(address, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

int APS5_VABI __asan_posix_memalign(void** memptr, std::size_t alignment, std::size_t size) {
    return AsanRuntime::Impl::__asan_posix_memalign_nid_no_patch(memptr, alignment, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_print_accumulated_stats() {
    return AsanRuntime::Impl::__asan_print_accumulated_stats_nid_no_patch({reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void* APS5_VABI __asan_realloc(void* pointer, std::size_t size) {
    return AsanRuntime::Impl::__asan_realloc_nid_no_patch(pointer, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

int APS5_VABI __asan_reallocalign() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uintptr_t APS5_VABI __asan_region_is_poisoned(std::uintptr_t address, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_region_is_poisoned_nid_no_patch(address, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_register_elf_globals(std::uintptr_t* flag, void* start, void* stop) {
    return AsanRuntime::Impl::__asan_register_elf_globals_nid_no_patch(flag, start, stop, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_register_globals(void* globals, std::uintptr_t count) {
    return AsanRuntime::Impl::__asan_register_globals_nid_no_patch(globals, count, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

int APS5_VABI __asan_register_image_globals() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI __asan_report_error(void* pc, void* bp, void* sp, void* address, int isWrite, std::size_t accessSize, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_report_error_nid_no_patch(pc, bp, sp, address, isWrite, accessSize, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_exp_load1(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_report_exp_load1_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_exp_load16(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_report_exp_load16_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_exp_load16_noabort(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_report_exp_load16_noabort_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_exp_load1_noabort(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_report_exp_load1_noabort_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_exp_load2(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_report_exp_load2_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_exp_load2_noabort(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_report_exp_load2_noabort_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_exp_load4(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_report_exp_load4_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_exp_load4_noabort(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_report_exp_load4_noabort_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_exp_load8(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_report_exp_load8_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_exp_load8_noabort(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_report_exp_load8_noabort_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_exp_load_n(std::uintptr_t address, std::uintptr_t size, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_report_exp_load_n_nid_no_patch(address, size, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_exp_load_n_noabort(std::uintptr_t address, std::uintptr_t size, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_report_exp_load_n_noabort_nid_no_patch(address, size, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_exp_store1(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_report_exp_store1_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_exp_store16(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_report_exp_store16_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_exp_store16_noabort(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_report_exp_store16_noabort_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_exp_store1_noabort(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_report_exp_store1_noabort_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_exp_store2(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_report_exp_store2_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_exp_store2_noabort(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_report_exp_store2_noabort_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_exp_store4(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_report_exp_store4_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_exp_store4_noabort(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_report_exp_store4_noabort_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_exp_store8(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_report_exp_store8_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_exp_store8_noabort(std::uintptr_t address, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_report_exp_store8_noabort_nid_no_patch(address, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_exp_store_n(std::uintptr_t address, std::uintptr_t size, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_report_exp_store_n_nid_no_patch(address, size, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_exp_store_n_noabort(std::uintptr_t address, std::uintptr_t size, std::uint32_t experiment) {
    return AsanRuntime::Impl::__asan_report_exp_store_n_noabort_nid_no_patch(address, size, experiment, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_load1(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_report_load1_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_load16(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_report_load16_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_load16_noabort(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_report_load16_noabort_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_load1_noabort(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_report_load1_noabort_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_load2(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_report_load2_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_load2_noabort(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_report_load2_noabort_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_load4(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_report_load4_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_load4_noabort(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_report_load4_noabort_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_load8(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_report_load8_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_load8_noabort(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_report_load8_noabort_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_load_n(std::uintptr_t address, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_report_load_n_nid_no_patch(address, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_load_n_noabort(std::uintptr_t address, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_report_load_n_noabort_nid_no_patch(address, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

int APS5_VABI __asan_report_present() {
    return AsanRuntime::Impl::__asan_report_present_nid_no_patch({reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_store1(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_report_store1_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_store16(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_report_store16_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_store16_noabort(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_report_store16_noabort_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_store1_noabort(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_report_store1_noabort_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_store2(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_report_store2_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_store2_noabort(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_report_store2_noabort_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_store4(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_report_store4_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_store4_noabort(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_report_store4_noabort_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_store8(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_report_store8_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_store8_noabort(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_report_store8_noabort_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_store_n(std::uintptr_t address, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_report_store_n_nid_no_patch(address, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_report_store_n_noabort(std::uintptr_t address, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_report_store_n_noabort_nid_no_patch(address, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_set_error_report_callback(void (APS5_VABI* callback)(const char*)) {
    return AsanRuntime::Impl::__asan_set_error_report_callback_nid_no_patch(callback, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_set_shadow_00(std::uintptr_t address, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_set_shadow_00_nid_no_patch(address, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_set_shadow_01(std::uintptr_t address, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_set_shadow_01_nid_no_patch(address, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_set_shadow_02(std::uintptr_t address, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_set_shadow_02_nid_no_patch(address, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_set_shadow_03(std::uintptr_t address, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_set_shadow_03_nid_no_patch(address, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_set_shadow_04(std::uintptr_t address, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_set_shadow_04_nid_no_patch(address, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_set_shadow_05(std::uintptr_t address, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_set_shadow_05_nid_no_patch(address, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_set_shadow_06(std::uintptr_t address, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_set_shadow_06_nid_no_patch(address, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_set_shadow_07(std::uintptr_t address, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_set_shadow_07_nid_no_patch(address, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_set_shadow_f1(std::uintptr_t address, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_set_shadow_f1_nid_no_patch(address, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_set_shadow_f2(std::uintptr_t address, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_set_shadow_f2_nid_no_patch(address, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_set_shadow_f3(std::uintptr_t address, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_set_shadow_f3_nid_no_patch(address, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_set_shadow_f5(std::uintptr_t address, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_set_shadow_f5_nid_no_patch(address, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_set_shadow_f8(std::uintptr_t address, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_set_shadow_f8_nid_no_patch(address, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_stack_free_0(std::uintptr_t pointer, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_free_0_nid_no_patch(pointer, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_stack_free_1(std::uintptr_t pointer, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_free_1_nid_no_patch(pointer, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_stack_free_10(std::uintptr_t pointer, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_free_10_nid_no_patch(pointer, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_stack_free_2(std::uintptr_t pointer, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_free_2_nid_no_patch(pointer, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_stack_free_3(std::uintptr_t pointer, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_free_3_nid_no_patch(pointer, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_stack_free_4(std::uintptr_t pointer, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_free_4_nid_no_patch(pointer, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_stack_free_5(std::uintptr_t pointer, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_free_5_nid_no_patch(pointer, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_stack_free_6(std::uintptr_t pointer, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_free_6_nid_no_patch(pointer, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_stack_free_7(std::uintptr_t pointer, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_free_7_nid_no_patch(pointer, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_stack_free_8(std::uintptr_t pointer, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_free_8_nid_no_patch(pointer, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_stack_free_9(std::uintptr_t pointer, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_free_9_nid_no_patch(pointer, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uintptr_t APS5_VABI __asan_stack_malloc_0(std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_malloc_0_nid_no_patch(size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uintptr_t APS5_VABI __asan_stack_malloc_1(std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_malloc_1_nid_no_patch(size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uintptr_t APS5_VABI __asan_stack_malloc_10(std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_malloc_10_nid_no_patch(size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uintptr_t APS5_VABI __asan_stack_malloc_2(std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_malloc_2_nid_no_patch(size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uintptr_t APS5_VABI __asan_stack_malloc_3(std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_malloc_3_nid_no_patch(size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uintptr_t APS5_VABI __asan_stack_malloc_4(std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_malloc_4_nid_no_patch(size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uintptr_t APS5_VABI __asan_stack_malloc_5(std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_malloc_5_nid_no_patch(size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uintptr_t APS5_VABI __asan_stack_malloc_6(std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_malloc_6_nid_no_patch(size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uintptr_t APS5_VABI __asan_stack_malloc_7(std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_malloc_7_nid_no_patch(size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uintptr_t APS5_VABI __asan_stack_malloc_8(std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_malloc_8_nid_no_patch(size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uintptr_t APS5_VABI __asan_stack_malloc_9(std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_malloc_9_nid_no_patch(size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uintptr_t APS5_VABI __asan_stack_malloc_always_0(std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_malloc_always_0_nid_no_patch(size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uintptr_t APS5_VABI __asan_stack_malloc_always_1(std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_malloc_always_1_nid_no_patch(size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uintptr_t APS5_VABI __asan_stack_malloc_always_10(std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_malloc_always_10_nid_no_patch(size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uintptr_t APS5_VABI __asan_stack_malloc_always_2(std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_malloc_always_2_nid_no_patch(size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uintptr_t APS5_VABI __asan_stack_malloc_always_3(std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_malloc_always_3_nid_no_patch(size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uintptr_t APS5_VABI __asan_stack_malloc_always_4(std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_malloc_always_4_nid_no_patch(size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uintptr_t APS5_VABI __asan_stack_malloc_always_5(std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_malloc_always_5_nid_no_patch(size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uintptr_t APS5_VABI __asan_stack_malloc_always_6(std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_malloc_always_6_nid_no_patch(size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uintptr_t APS5_VABI __asan_stack_malloc_always_7(std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_malloc_always_7_nid_no_patch(size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uintptr_t APS5_VABI __asan_stack_malloc_always_8(std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_malloc_always_8_nid_no_patch(size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uintptr_t APS5_VABI __asan_stack_malloc_always_9(std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_stack_malloc_always_9_nid_no_patch(size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_store1(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_store1_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_store16(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_store16_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_store16_noabort(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_store16_noabort_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_store1_noabort(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_store1_noabort_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_store2(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_store2_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_store2_noabort(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_store2_noabort_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_store4(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_store4_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_store4_noabort(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_store4_noabort_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_store8(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_store8_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_store8_noabort(std::uintptr_t address) {
    return AsanRuntime::Impl::__asan_store8_noabort_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_storeN(std::uintptr_t address, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_storeN_nid_no_patch(address, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_storeN_noabort(std::uintptr_t address, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_storeN_noabort_nid_no_patch(address, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_unpoison_intra_object_redzone(std::uintptr_t address, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_unpoison_intra_object_redzone_nid_no_patch(address, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_unpoison_memory_region(const volatile void* addr, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_unpoison_memory_region_nid_no_patch(addr, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_unpoison_stack_memory(std::uintptr_t address, std::uintptr_t size) {
    return AsanRuntime::Impl::__asan_unpoison_stack_memory_nid_no_patch(address, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_unregister_elf_globals(std::uintptr_t* flag, void* start, void* stop) {
    return AsanRuntime::Impl::__asan_unregister_elf_globals_nid_no_patch(flag, start, stop, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_unregister_globals(void* globals, std::uintptr_t count) {
    return AsanRuntime::Impl::__asan_unregister_globals_nid_no_patch(globals, count, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

int APS5_VABI __asan_unregister_image_globals() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __asan_update_allocation_context(void* address) {
    return AsanRuntime::Impl::__asan_update_allocation_context_nid_no_patch(address, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __asan_version_mismatch_check_v6() {}

void APS5_VABI __asan_version_mismatch_check_v7() {}

void APS5_VABI __asan_version_mismatch_check_v8() {}

const char* APS5_VABI __sancov_default_options() {
    return "";
}

void APS5_VABI __sanitizer_annotate_contiguous_container(const void* beg, const void* end, const void* oldMid, const void* newMid) {
    return AsanRuntime::Impl::__sanitizer_annotate_contiguous_container_nid_no_patch(beg, end, oldMid, newMid, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __sanitizer_annotate_double_ended_contiguous_container(const void* storageBeg, const void* storageEnd, const void* oldContainerBeg, const void* oldContainerEnd, const void* newContainerBeg, const void* newContainerEnd) {
    return AsanRuntime::Impl::__sanitizer_annotate_double_ended_contiguous_container_nid_no_patch(storageBeg, storageEnd, oldContainerBeg, oldContainerEnd, newContainerBeg, newContainerEnd, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

const void* APS5_VABI __sanitizer_contiguous_container_find_bad_address(const void* beg, const void* mid, const void* end) {
    return AsanRuntime::Impl::__sanitizer_contiguous_container_find_bad_address_nid_no_patch(beg, mid, end, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __sanitizer_cov_dump() {
    return AsanRuntime::Impl::__sanitizer_cov_dump_nid_no_patch({reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __sanitizer_cov_trace_pc_guard(std::uint32_t* guard) {
    return AsanRuntime::Impl::__sanitizer_cov_trace_pc_guard_nid_no_patch(guard, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __sanitizer_cov_trace_pc_guard_init(std::uint32_t* start, std::uint32_t* end) {
    return AsanRuntime::Impl::__sanitizer_cov_trace_pc_guard_init_nid_no_patch(start, end, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

const void* APS5_VABI __sanitizer_double_ended_contiguous_container_find_bad_address(const void* storageBeg, const void* containerBeg, const void* containerEnd, const void* storageEnd) {
    return AsanRuntime::Impl::__sanitizer_double_ended_contiguous_container_find_bad_address_nid_no_patch(storageBeg, containerBeg, containerEnd, storageEnd, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __sanitizer_dump_coverage(const std::uintptr_t* pcs, std::uintptr_t length) {
    return AsanRuntime::Impl::__sanitizer_dump_coverage_nid_no_patch(pcs, length, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __sanitizer_dump_trace_pc_guard_coverage() {
    return AsanRuntime::Impl::__sanitizer_dump_trace_pc_guard_coverage_nid_no_patch({reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __sanitizer_finish_switch_fiber(void* fakeStack, const void** bottomOld, std::uintptr_t* sizeOld) {
    return AsanRuntime::Impl::__sanitizer_finish_switch_fiber_nid_no_patch(fakeStack, bottomOld, sizeOld, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __sanitizer_free_hook(void*) {
}

const void* APS5_VABI __sanitizer_get_allocated_begin(const void* pointer) {
    return AsanRuntime::Impl::__sanitizer_get_allocated_begin_nid_no_patch(pointer, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::size_t APS5_VABI __sanitizer_get_allocated_size(const void* pointer) {
    return AsanRuntime::Impl::__sanitizer_get_allocated_size_nid_no_patch(pointer, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::size_t APS5_VABI __sanitizer_get_allocated_size_fast(const void* pointer) {
    return AsanRuntime::Impl::__sanitizer_get_allocated_size_fast_nid_no_patch(pointer, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::size_t APS5_VABI __sanitizer_get_current_allocated_bytes() {
    return AsanRuntime::Impl::__sanitizer_get_current_allocated_bytes_nid_no_patch({reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::size_t APS5_VABI __sanitizer_get_estimated_allocated_size(std::size_t size) {
    return AsanRuntime::Impl::__sanitizer_get_estimated_allocated_size_nid_no_patch(size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::size_t APS5_VABI __sanitizer_get_free_bytes() {
    return AsanRuntime::Impl::__sanitizer_get_free_bytes_nid_no_patch({reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::size_t APS5_VABI __sanitizer_get_heap_size() {
    return AsanRuntime::Impl::__sanitizer_get_heap_size_nid_no_patch({reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

int APS5_VABI __sanitizer_get_ownership(const void* pointer) {
    return AsanRuntime::Impl::__sanitizer_get_ownership_nid_no_patch(pointer, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

const char* APS5_VABI __sanitizer_get_report_path() {
    return AsanRuntime::Impl::__sanitizer_get_report_path_nid_no_patch({reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::size_t APS5_VABI __sanitizer_get_unmapped_bytes() {
    return 0;
}

void APS5_VABI __sanitizer_malloc_hook(void*, std::size_t) {
}

void APS5_VABI __sanitizer_print_stack_trace() {
    return AsanRuntime::Impl::__sanitizer_print_stack_trace_nid_no_patch({reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __sanitizer_ptr_cmp(void*, void*) {

}

void APS5_VABI __sanitizer_ptr_sub(void*, void*) {

}

void APS5_VABI __sanitizer_purge_allocator() {
    return AsanRuntime::Impl::__sanitizer_purge_allocator_nid_no_patch({reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __sanitizer_sandbox_on_notify(void* arguments) {
    return AsanRuntime::Impl::__sanitizer_sandbox_on_notify_nid_no_patch(arguments, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __sanitizer_set_death_callback(void (APS5_VABI* callback)()) {
    return AsanRuntime::Impl::__sanitizer_set_death_callback_nid_no_patch(callback, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __sanitizer_set_report_fd(void* descriptor) {
    return AsanRuntime::Impl::__sanitizer_set_report_fd_nid_no_patch(descriptor, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __sanitizer_set_report_path(const char* path) {
    return AsanRuntime::Impl::__sanitizer_set_report_path_nid_no_patch(path, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __sanitizer_start_switch_fiber(void** fakeStackSave, const void* bottom, std::uintptr_t size) {
    return AsanRuntime::Impl::__sanitizer_start_switch_fiber_nid_no_patch(fakeStackSave, bottom, size, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __sanitizer_symbolize_global(std::uintptr_t address, const char* format, char* output, std::uintptr_t outputSize) {
    return AsanRuntime::Impl::__sanitizer_symbolize_global_nid_no_patch(address, format, output, outputSize, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __sanitizer_symbolize_pc(std::uintptr_t pc, const char* format, char* output, std::uintptr_t outputSize) {
    return AsanRuntime::Impl::__sanitizer_symbolize_pc_nid_no_patch(pc, format, output, outputSize, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uint16_t APS5_VABI __sanitizer_unaligned_load16(const void* ptr) {
    return AsanRuntime::Impl::__sanitizer_unaligned_load16_nid_no_patch(ptr, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uint32_t APS5_VABI __sanitizer_unaligned_load32(const void* ptr) {
    return AsanRuntime::Impl::__sanitizer_unaligned_load32_nid_no_patch(ptr, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

std::uint64_t APS5_VABI __sanitizer_unaligned_load64(const void* ptr) {
    return AsanRuntime::Impl::__sanitizer_unaligned_load64_nid_no_patch(ptr, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __sanitizer_unaligned_store16(void* ptr, std::uint16_t val) {
    return AsanRuntime::Impl::__sanitizer_unaligned_store16_nid_no_patch(ptr, val, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __sanitizer_unaligned_store32(void* ptr, std::uint32_t val) {
    return AsanRuntime::Impl::__sanitizer_unaligned_store32_nid_no_patch(ptr, val, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __sanitizer_unaligned_store64(void* ptr, std::uint64_t val) {
    return AsanRuntime::Impl::__sanitizer_unaligned_store64_nid_no_patch(ptr, val, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

int APS5_VABI __sanitizer_verify_contiguous_container(const void* beg, const void* mid, const void* end) {
    return AsanRuntime::Impl::__sanitizer_verify_contiguous_container_nid_no_patch(beg, mid, end, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

int APS5_VABI __sanitizer_verify_double_ended_contiguous_container(const void* storageBeg, const void* containerBeg, const void* containerEnd, const void* storageEnd) {
    return AsanRuntime::Impl::__sanitizer_verify_double_ended_contiguous_container_nid_no_patch(storageBeg, containerBeg, containerEnd, storageEnd, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

int APS5_VABI __ubsan_current_error_report() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __ubsan_current_error_report_size() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

const char* APS5_VABI __ubsan_default_options() {
    return "";
}

void APS5_VABI __ubsan_get_current_report_data(const char** issueKind, const char** message, const char** filename, unsigned* line, unsigned* column, char** memoryAddress) {
    return AsanRuntime::Impl::__ubsan_get_current_report_data_nid_no_patch(issueKind, message, filename, line, column, memoryAddress, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_add_overflow(void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    return AsanRuntime::Impl::__ubsan_handle_add_overflow_nid_no_patch(data, lhs, rhs, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_add_overflow_abort(void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    return AsanRuntime::Impl::__ubsan_handle_add_overflow_abort_nid_no_patch(data, lhs, rhs, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_alignment_assumption(void* data, std::uintptr_t pointer, std::uintptr_t alignment, std::uintptr_t offset) {
    return AsanRuntime::Impl::__ubsan_handle_alignment_assumption_nid_no_patch(data, pointer, alignment, offset, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_alignment_assumption_abort(void* data, std::uintptr_t pointer, std::uintptr_t alignment, std::uintptr_t offset) {
    return AsanRuntime::Impl::__ubsan_handle_alignment_assumption_abort_nid_no_patch(data, pointer, alignment, offset, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_builtin_unreachable(void* data) {
    return AsanRuntime::Impl::__ubsan_handle_builtin_unreachable_nid_no_patch(data, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_divrem_overflow(void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    return AsanRuntime::Impl::__ubsan_handle_divrem_overflow_nid_no_patch(data, lhs, rhs, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_divrem_overflow_abort(void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    return AsanRuntime::Impl::__ubsan_handle_divrem_overflow_abort_nid_no_patch(data, lhs, rhs, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

int APS5_VABI __ubsan_handle_dynamic_type_cache_miss() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __ubsan_handle_dynamic_type_cache_miss_abort() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI __ubsan_handle_float_cast_overflow(void* data, std::uintptr_t from) {
    return AsanRuntime::Impl::__ubsan_handle_float_cast_overflow_nid_no_patch(data, from, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_float_cast_overflow_abort(void* data, std::uintptr_t from) {
    return AsanRuntime::Impl::__ubsan_handle_float_cast_overflow_abort_nid_no_patch(data, from, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_function_type_mismatch(void* data, std::uintptr_t function) {
    return AsanRuntime::Impl::__ubsan_handle_function_type_mismatch_nid_no_patch(data, function, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_function_type_mismatch_abort(void* data, std::uintptr_t function) {
    return AsanRuntime::Impl::__ubsan_handle_function_type_mismatch_abort_nid_no_patch(data, function, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

int APS5_VABI __ubsan_handle_function_type_mismatch_v1() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI __ubsan_handle_function_type_mismatch_v1_abort() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI __ubsan_handle_implicit_conversion(void* data, std::uintptr_t source, std::uintptr_t destination) {
    return AsanRuntime::Impl::__ubsan_handle_implicit_conversion_nid_no_patch(data, source, destination, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_implicit_conversion_abort(void* data, std::uintptr_t source, std::uintptr_t destination) {
    return AsanRuntime::Impl::__ubsan_handle_implicit_conversion_abort_nid_no_patch(data, source, destination, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_invalid_builtin(void* data) {
    return AsanRuntime::Impl::__ubsan_handle_invalid_builtin_nid_no_patch(data, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_invalid_builtin_abort(void* data) {
    return AsanRuntime::Impl::__ubsan_handle_invalid_builtin_abort_nid_no_patch(data, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_load_invalid_value(void* data, std::uintptr_t value) {
    return AsanRuntime::Impl::__ubsan_handle_load_invalid_value_nid_no_patch(data, value, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_load_invalid_value_abort(void* data, std::uintptr_t value) {
    return AsanRuntime::Impl::__ubsan_handle_load_invalid_value_abort_nid_no_patch(data, value, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_missing_return(void* data) {
    return AsanRuntime::Impl::__ubsan_handle_missing_return_nid_no_patch(data, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_mul_overflow(void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    return AsanRuntime::Impl::__ubsan_handle_mul_overflow_nid_no_patch(data, lhs, rhs, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_mul_overflow_abort(void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    return AsanRuntime::Impl::__ubsan_handle_mul_overflow_abort_nid_no_patch(data, lhs, rhs, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_negate_overflow(void* data, std::uintptr_t oldValue) {
    return AsanRuntime::Impl::__ubsan_handle_negate_overflow_nid_no_patch(data, oldValue, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_negate_overflow_abort(void* data, std::uintptr_t oldValue) {
    return AsanRuntime::Impl::__ubsan_handle_negate_overflow_abort_nid_no_patch(data, oldValue, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_nonnull_arg(void* data) {
    return AsanRuntime::Impl::__ubsan_handle_nonnull_arg_nid_no_patch(data, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_nonnull_arg_abort(void* data) {
    return AsanRuntime::Impl::__ubsan_handle_nonnull_arg_abort_nid_no_patch(data, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_nonnull_return(void* data) {
    return AsanRuntime::Impl::__ubsan_handle_nonnull_return_nid_no_patch(data, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_nonnull_return_abort(void* data) {
    return AsanRuntime::Impl::__ubsan_handle_nonnull_return_abort_nid_no_patch(data, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_nonnull_return_v1(void* data, void* location) {
    return AsanRuntime::Impl::__ubsan_handle_nonnull_return_v1_nid_no_patch(data, location, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_nonnull_return_v1_abort(void* data, void* location) {
    return AsanRuntime::Impl::__ubsan_handle_nonnull_return_v1_abort_nid_no_patch(data, location, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_nullability_arg(void* data) {
    return AsanRuntime::Impl::__ubsan_handle_nullability_arg_nid_no_patch(data, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_nullability_arg_abort(void* data) {
    return AsanRuntime::Impl::__ubsan_handle_nullability_arg_abort_nid_no_patch(data, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_nullability_return_v1(void* data, void* location) {
    return AsanRuntime::Impl::__ubsan_handle_nullability_return_v1_nid_no_patch(data, location, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_nullability_return_v1_abort(void* data, void* location) {
    return AsanRuntime::Impl::__ubsan_handle_nullability_return_v1_abort_nid_no_patch(data, location, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_out_of_bounds(void* data, std::uintptr_t index) {
    return AsanRuntime::Impl::__ubsan_handle_out_of_bounds_nid_no_patch(data, index, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_out_of_bounds_abort(void* data, std::uintptr_t index) {
    return AsanRuntime::Impl::__ubsan_handle_out_of_bounds_abort_nid_no_patch(data, index, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_pointer_overflow(void* data, std::uintptr_t base, std::uintptr_t result) {
    return AsanRuntime::Impl::__ubsan_handle_pointer_overflow_nid_no_patch(data, base, result, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_pointer_overflow_abort(void* data, std::uintptr_t base, std::uintptr_t result) {
    return AsanRuntime::Impl::__ubsan_handle_pointer_overflow_abort_nid_no_patch(data, base, result, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_shift_out_of_bounds(void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    return AsanRuntime::Impl::__ubsan_handle_shift_out_of_bounds_nid_no_patch(data, lhs, rhs, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_shift_out_of_bounds_abort(void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    return AsanRuntime::Impl::__ubsan_handle_shift_out_of_bounds_abort_nid_no_patch(data, lhs, rhs, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_sub_overflow(void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    return AsanRuntime::Impl::__ubsan_handle_sub_overflow_nid_no_patch(data, lhs, rhs, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_sub_overflow_abort(void* data, std::uintptr_t lhs, std::uintptr_t rhs) {
    return AsanRuntime::Impl::__ubsan_handle_sub_overflow_abort_nid_no_patch(data, lhs, rhs, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_type_mismatch(void* data, std::uintptr_t pointer) {
    return AsanRuntime::Impl::__ubsan_handle_type_mismatch_nid_no_patch(data, pointer, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_type_mismatch_abort(void* data, std::uintptr_t pointer) {
    return AsanRuntime::Impl::__ubsan_handle_type_mismatch_abort_nid_no_patch(data, pointer, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_type_mismatch_v1(void* data, std::uintptr_t pointer) {
    return AsanRuntime::Impl::__ubsan_handle_type_mismatch_v1_nid_no_patch(data, pointer, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_type_mismatch_v1_abort(void* data, std::uintptr_t pointer) {
    return AsanRuntime::Impl::__ubsan_handle_type_mismatch_v1_abort_nid_no_patch(data, pointer, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_vla_bound_not_positive(void* data, std::uintptr_t bound) {
    return AsanRuntime::Impl::__ubsan_handle_vla_bound_not_positive_nid_no_patch(data, bound, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

void APS5_VABI __ubsan_handle_vla_bound_not_positive_abort(void* data, std::uintptr_t bound) {
    return AsanRuntime::Impl::__ubsan_handle_vla_bound_not_positive_abort_nid_no_patch(data, bound, {reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)), reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)), __func__});
}

}
