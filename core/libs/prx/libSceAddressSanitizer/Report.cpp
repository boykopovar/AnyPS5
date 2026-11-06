#include "prx/libSceAddressSanitizer/include/Api.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace {

struct ReportState {
    std::atomic<bool> present{false};
    std::atomic<std::uintptr_t> pc{0};
    std::atomic<std::uintptr_t> bp{0};
    std::atomic<std::uintptr_t> sp{0};
    std::atomic<std::uintptr_t> address{0};
    std::atomic<int> accessType{0};
    std::atomic<std::size_t> accessSize{0};
    std::atomic<std::uint64_t> expected{0};
    char description[32] = "none";
};

ReportState state;
std::atomic<void (*)(const char*)> errorCallback{nullptr};

void Record(std::uintptr_t pc, std::uintptr_t bp, std::uintptr_t sp, std::uintptr_t address, int isWrite,
            std::size_t accessSize, bool hasExpected, std::uint64_t expected) {
    state.pc.store(pc, std::memory_order_relaxed);
    state.bp.store(bp, std::memory_order_relaxed);
    state.sp.store(sp, std::memory_order_relaxed);
    state.address.store(address, std::memory_order_relaxed);
    state.accessType.store(isWrite, std::memory_order_relaxed);
    state.accessSize.store(accessSize, std::memory_order_relaxed);
    state.expected.store(hasExpected ? expected : 0, std::memory_order_relaxed);
    std::snprintf(state.description, sizeof(state.description), "%s of size %zu", isWrite ? "write" : "read", accessSize);
    state.present.store(true, std::memory_order_release);
    if (const auto callback = errorCallback.load(std::memory_order_acquire)) callback(state.description);
}

void RecordAccess(std::uintptr_t address, std::size_t size, bool isWrite) {
    Record(0, 0, 0, address, isWrite ? 1 : 0, size, false, 0);
}

void RecordExpected(std::uintptr_t address, std::size_t size, bool isWrite, std::uint64_t expected) {
    Record(0, 0, 0, address, isWrite ? 1 : 0, size, true, expected);
}

}

extern "C" {

void APS5_VABI __asan_report_error(void* pc, void* bp, void* sp, void* address, int isWrite, std::size_t accessSize) {
    Record(reinterpret_cast<std::uintptr_t>(pc), reinterpret_cast<std::uintptr_t>(bp), reinterpret_cast<std::uintptr_t>(sp),
           reinterpret_cast<std::uintptr_t>(address), isWrite, accessSize, false, 0);
}

int APS5_VABI __asan_report_present() {
    return state.present.load(std::memory_order_acquire) ? 1 : 0;
}

void* APS5_VABI __asan_get_report_pc() {
    return reinterpret_cast<void*>(state.pc.load(std::memory_order_relaxed));
}

void* APS5_VABI __asan_get_report_bp() {
    return reinterpret_cast<void*>(state.bp.load(std::memory_order_relaxed));
}

void* APS5_VABI __asan_get_report_sp() {
    return reinterpret_cast<void*>(state.sp.load(std::memory_order_relaxed));
}

void* APS5_VABI __asan_get_report_address() {
    return reinterpret_cast<void*>(state.address.load(std::memory_order_relaxed));
}

int APS5_VABI __asan_get_report_access_type() {
    return state.accessType.load(std::memory_order_relaxed);
}

std::size_t APS5_VABI __asan_get_report_access_size() {
    return state.accessSize.load(std::memory_order_relaxed);
}

const char* APS5_VABI __asan_get_report_description() {
    return state.description;
}

void APS5_VABI __asan_set_error_report_callback(void (*callback)(const char*)) {
    errorCallback.store(callback, std::memory_order_release);
}

void APS5_VABI __asan_describe_address(void*) {
}

char* APS5_VABI __asan_locate_address(void*, char* name, std::size_t nameSize, void** regionAddress, std::size_t* regionSize) {
    if (name != nullptr && nameSize != 0) name[0] = '\0';
    if (regionAddress != nullptr) *regionAddress = nullptr;
    if (regionSize != nullptr) *regionSize = 0;
    return nullptr;
}

void APS5_VABI __asan_report_load1(std::uintptr_t address) {
    RecordAccess(address, 1, false);
}

void APS5_VABI __asan_report_load1_noabort(std::uintptr_t address) {
    RecordAccess(address, 1, false);
}

void APS5_VABI __asan_report_load2(std::uintptr_t address) {
    RecordAccess(address, 2, false);
}

void APS5_VABI __asan_report_load2_noabort(std::uintptr_t address) {
    RecordAccess(address, 2, false);
}

void APS5_VABI __asan_report_load4(std::uintptr_t address) {
    RecordAccess(address, 4, false);
}

void APS5_VABI __asan_report_load4_noabort(std::uintptr_t address) {
    RecordAccess(address, 4, false);
}

void APS5_VABI __asan_report_load8(std::uintptr_t address) {
    RecordAccess(address, 8, false);
}

void APS5_VABI __asan_report_load8_noabort(std::uintptr_t address) {
    RecordAccess(address, 8, false);
}

void APS5_VABI __asan_report_load16(std::uintptr_t address) {
    RecordAccess(address, 16, false);
}

void APS5_VABI __asan_report_load16_noabort(std::uintptr_t address) {
    RecordAccess(address, 16, false);
}

void APS5_VABI __asan_report_load_n(std::uintptr_t address, std::size_t size) {
    RecordAccess(address, size, false);
}

void APS5_VABI __asan_report_load_n_noabort(std::uintptr_t address, std::size_t size) {
    RecordAccess(address, size, false);
}

void APS5_VABI __asan_report_store1(std::uintptr_t address) {
    RecordAccess(address, 1, true);
}

void APS5_VABI __asan_report_store1_noabort(std::uintptr_t address) {
    RecordAccess(address, 1, true);
}

void APS5_VABI __asan_report_store2(std::uintptr_t address) {
    RecordAccess(address, 2, true);
}

void APS5_VABI __asan_report_store2_noabort(std::uintptr_t address) {
    RecordAccess(address, 2, true);
}

void APS5_VABI __asan_report_store4(std::uintptr_t address) {
    RecordAccess(address, 4, true);
}

void APS5_VABI __asan_report_store4_noabort(std::uintptr_t address) {
    RecordAccess(address, 4, true);
}

void APS5_VABI __asan_report_store8(std::uintptr_t address) {
    RecordAccess(address, 8, true);
}

void APS5_VABI __asan_report_store8_noabort(std::uintptr_t address) {
    RecordAccess(address, 8, true);
}

void APS5_VABI __asan_report_store16(std::uintptr_t address) {
    RecordAccess(address, 16, true);
}

void APS5_VABI __asan_report_store16_noabort(std::uintptr_t address) {
    RecordAccess(address, 16, true);
}

void APS5_VABI __asan_report_store_n(std::uintptr_t address, std::size_t size) {
    RecordAccess(address, size, true);
}

void APS5_VABI __asan_report_store_n_noabort(std::uintptr_t address, std::size_t size) {
    RecordAccess(address, size, true);
}

void APS5_VABI __asan_report_exp_load1(std::uintptr_t address, std::uint32_t expected) {
    RecordExpected(address, 1, false, expected);
}

void APS5_VABI __asan_report_exp_load1_noabort(std::uintptr_t address, std::uint32_t expected) {
    RecordExpected(address, 1, false, expected);
}

void APS5_VABI __asan_report_exp_load2(std::uintptr_t address, std::uint32_t expected) {
    RecordExpected(address, 2, false, expected);
}

void APS5_VABI __asan_report_exp_load2_noabort(std::uintptr_t address, std::uint32_t expected) {
    RecordExpected(address, 2, false, expected);
}

void APS5_VABI __asan_report_exp_load4(std::uintptr_t address, std::uint32_t expected) {
    RecordExpected(address, 4, false, expected);
}

void APS5_VABI __asan_report_exp_load4_noabort(std::uintptr_t address, std::uint32_t expected) {
    RecordExpected(address, 4, false, expected);
}

void APS5_VABI __asan_report_exp_load8(std::uintptr_t address, std::uint64_t expected) {
    RecordExpected(address, 8, false, expected);
}

void APS5_VABI __asan_report_exp_load8_noabort(std::uintptr_t address, std::uint64_t expected) {
    RecordExpected(address, 8, false, expected);
}

void APS5_VABI __asan_report_exp_load16(std::uintptr_t address, std::uint64_t expected) {
    RecordExpected(address, 16, false, expected);
}

void APS5_VABI __asan_report_exp_load16_noabort(std::uintptr_t address, std::uint64_t expected) {
    RecordExpected(address, 16, false, expected);
}

void APS5_VABI __asan_report_exp_load_n(std::uintptr_t address, std::size_t size, std::uint32_t expected) {
    RecordExpected(address, size, false, expected);
}

void APS5_VABI __asan_report_exp_load_n_noabort(std::uintptr_t address, std::size_t size, std::uint32_t expected) {
    RecordExpected(address, size, false, expected);
}

void APS5_VABI __asan_report_exp_store1(std::uintptr_t address, std::uint32_t expected) {
    RecordExpected(address, 1, true, expected);
}

void APS5_VABI __asan_report_exp_store1_noabort(std::uintptr_t address, std::uint32_t expected) {
    RecordExpected(address, 1, true, expected);
}

void APS5_VABI __asan_report_exp_store2(std::uintptr_t address, std::uint32_t expected) {
    RecordExpected(address, 2, true, expected);
}

void APS5_VABI __asan_report_exp_store2_noabort(std::uintptr_t address, std::uint32_t expected) {
    RecordExpected(address, 2, true, expected);
}

void APS5_VABI __asan_report_exp_store4(std::uintptr_t address, std::uint32_t expected) {
    RecordExpected(address, 4, true, expected);
}

void APS5_VABI __asan_report_exp_store4_noabort(std::uintptr_t address, std::uint32_t expected) {
    RecordExpected(address, 4, true, expected);
}

void APS5_VABI __asan_report_exp_store8(std::uintptr_t address, std::uint64_t expected) {
    RecordExpected(address, 8, true, expected);
}

void APS5_VABI __asan_report_exp_store8_noabort(std::uintptr_t address, std::uint64_t expected) {
    RecordExpected(address, 8, true, expected);
}

void APS5_VABI __asan_report_exp_store16(std::uintptr_t address, std::uint64_t expected) {
    RecordExpected(address, 16, true, expected);
}

void APS5_VABI __asan_report_exp_store16_noabort(std::uintptr_t address, std::uint64_t expected) {
    RecordExpected(address, 16, true, expected);
}

void APS5_VABI __asan_report_exp_store_n(std::uintptr_t address, std::size_t size, std::uint32_t expected) {
    RecordExpected(address, size, true, expected);
}

void APS5_VABI __asan_report_exp_store_n_noabort(std::uintptr_t address, std::size_t size, std::uint32_t expected) {
    RecordExpected(address, size, true, expected);
}

}
