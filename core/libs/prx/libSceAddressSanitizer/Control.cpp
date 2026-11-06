#include "prx/libSceAddressSanitizer/include/Api.hpp"
#include "prx/libc/include/Api.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

std::atomic<bool> initialized{false};
std::atomic<std::size_t> registeredGlobals{0};
std::atomic<std::uint32_t> coverageGuard{0};
std::atomic<void (*)()> deathCallback{nullptr};
std::atomic<void*> reportDescriptor{nullptr};
std::atomic<bool> hasReportPath{false};
char reportPath[4096] = "";

const char* const noOptions = "";
const char* const noPath = "";

void Prepare(const char* name) {
    if (!initialized.load(std::memory_order_acquire)) initialized.store(true, std::memory_order_release);
    (void)name;
}

}

extern "C" {

void APS5_VABI __asan_init() {
    Prepare(nullptr);
}

void APS5_VABI __asan_init_v5() {
    Prepare(nullptr);
}

void APS5_VABI __asan_version_mismatch_check_v6() {
}

void APS5_VABI __asan_version_mismatch_check_v7() {
}

void APS5_VABI __asan_version_mismatch_check_v8() {
}

void APS5_VABI __asan_before_dynamic_init(const char* moduleName) {
    Prepare(moduleName);
}

void APS5_VABI __asan_after_dynamic_init() {
    Prepare(nullptr);
}

void APS5_VABI __asan_register_globals(const void*, std::size_t count) {
    registeredGlobals.fetch_add(count, std::memory_order_relaxed);
}

void APS5_VABI __asan_unregister_globals(const void*, std::size_t count) {
    auto current = registeredGlobals.load(std::memory_order_relaxed);
    while (true) {
        const auto next = current > count ? current - count : 0;
        if (registeredGlobals.compare_exchange_weak(current, next, std::memory_order_relaxed)) return;
    }
}

void APS5_VABI __asan_register_elf_globals(std::uintptr_t* flag, const void*, const void*) {
    if (flag != nullptr) *flag = 1;
}

void APS5_VABI __asan_unregister_elf_globals(std::uintptr_t* flag, const void*, const void*) {
    if (flag != nullptr) *flag = 0;
}

void APS5_VABI __asan_register_image_globals(std::uintptr_t* flag) {
    if (flag != nullptr) *flag = 1;
}

void APS5_VABI __asan_unregister_image_globals(std::uintptr_t* flag) {
    if (flag != nullptr) *flag = 0;
}

void APS5_VABI __asan_handle_no_return() {
}

void APS5_VABI __sanitizer_start_switch_fiber(void** fakeStackSave, const void*, std::size_t) {
    if (fakeStackSave != nullptr) *fakeStackSave = nullptr;
}

void APS5_VABI __sanitizer_finish_switch_fiber(void*, const void** oldBottom, std::size_t* oldSize) {
    if (oldBottom != nullptr) *oldBottom = nullptr;
    if (oldSize != nullptr) *oldSize = 0;
}

void APS5_VABI __sanitizer_sandbox_on_notify(void*) {
}

void APS5_VABI __sanitizer_malloc_hook(const volatile void*, std::size_t) {
}

void APS5_VABI __sanitizer_free_hook(const volatile void*) {
}

void APS5_VABI __sanitizer_purge_allocator() {
}

int APS5_VABI __sanitizer_get_ownership(const void* pointer) {
    return pointer != nullptr ? 1 : 0;
}

void* APS5_VABI __sanitizer_get_allocated_begin(const void* pointer) {
    return const_cast<void*>(pointer);
}

std::size_t APS5_VABI __sanitizer_get_allocated_size(const void* pointer) {
    if (pointer == nullptr) return 0;
    return malloc_usable_size_nid_postfix(const_cast<void*>(pointer));
}

std::size_t APS5_VABI __sanitizer_get_allocated_size_fast(const void* pointer) {
    return __sanitizer_get_allocated_size(pointer);
}

std::size_t APS5_VABI __sanitizer_get_estimated_allocated_size(std::size_t size) {
    return size;
}

void APS5_VABI __sanitizer_set_death_callback(void (*callback)()) {
    deathCallback.store(callback, std::memory_order_release);
}

void APS5_VABI __sanitizer_set_report_fd(void* descriptor) {
    reportDescriptor.store(descriptor, std::memory_order_release);
}

void APS5_VABI __sanitizer_set_report_path(const char* path) {
    if (path == nullptr) {
        hasReportPath.store(false, std::memory_order_release);
        reportPath[0] = '\0';
        return;
    }
    std::strncpy(reportPath, path, sizeof(reportPath) - 1);
    reportPath[sizeof(reportPath) - 1] = '\0';
    hasReportPath.store(true, std::memory_order_release);
}

const char* APS5_VABI __sanitizer_get_report_path() {
    return hasReportPath.load(std::memory_order_acquire) ? reportPath : noPath;
}

void APS5_VABI __sanitizer_print_stack_trace() {
}

void APS5_VABI __sanitizer_symbolize_pc(void*, const char*, char* output, std::size_t outputSize) {
    if (output != nullptr && outputSize != 0) output[0] = '\0';
}

void APS5_VABI __sanitizer_symbolize_global(void*, const char*, char* output, std::size_t outputSize) {
    if (output != nullptr && outputSize != 0) output[0] = '\0';
}

const char* APS5_VABI __sancov_default_options() {
    return noOptions;
}

void APS5_VABI __sanitizer_cov_trace_pc_guard(std::uint32_t*) {
}

void APS5_VABI __sanitizer_cov_trace_pc_guard_init(std::uint32_t* start, std::uint32_t* stop) {
    if (start == nullptr || stop == nullptr || start == stop || *start != 0) return;
    for (auto* guard = start; guard < stop; ++guard) *guard = coverageGuard.fetch_add(1, std::memory_order_relaxed) + 1;
}

void APS5_VABI __sanitizer_cov_dump() {
}

void APS5_VABI __sanitizer_dump_coverage(const std::uintptr_t*, std::uintptr_t) {
}

void APS5_VABI __sanitizer_dump_trace_pc_guard_coverage() {
}

void APS5_VABI __sanitizer_annotate_contiguous_container(const void*, const void*, const void*, const void*) {
}

int APS5_VABI __sanitizer_verify_contiguous_container(const void* begin, const void* middle, const void* end) {
    const auto first = reinterpret_cast<std::uintptr_t>(begin);
    const auto second = reinterpret_cast<std::uintptr_t>(middle);
    const auto third = reinterpret_cast<std::uintptr_t>(end);
    return first <= second && second <= third ? 1 : 0;
}

const void* APS5_VABI __sanitizer_contiguous_container_find_bad_address(const void*, const void*, const void*) {
    return nullptr;
}

}
