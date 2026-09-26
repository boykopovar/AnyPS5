#include <cstdint>
#include <cstddef>
#include <array>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Pthread/include/ThreadLifecycle.hpp"

namespace {

std::mutex rtldHeapMutex;
std::array<void*, 10> rtldHeapApi{};

void registerApplicationHeapApi(void* const* api) {
    if (api == nullptr)
        throw std::invalid_argument("RTLD application heap: null allocator API");
    std::array<void*, 10> replacement;
    std::memcpy(replacement.data(), api, sizeof(replacement));
    if (replacement[0] == nullptr || replacement[1] == nullptr)
        throw std::invalid_argument("RTLD application heap: malloc and free are required");
    std::lock_guard lock(rtldHeapMutex);
    if (rtldHeapApi[0] != nullptr && rtldHeapApi != replacement)
        throw std::runtime_error("RTLD application heap: cannot replace an active allocator");
    rtldHeapApi = replacement;
}

}

extern "C" {

void APS5_VABI sceKernelRtldSetApplicationHeapAPI(void* api[]) {
    registerApplicationHeapApi(api);
}

int APS5_VABI sceKernelRtldThreadAtexitDecrement(uint64_t* c) {
 (void)c;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelRtldThreadAtexitIncrement(uint64_t* c) {
 (void)c;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void APS5_VABI sceKernelSetThreadAtexitCount(get_thread_atexit_count_func_t func) {
    ThreadLifecycle::SetThreadAtexitCount(func);
}

void APS5_VABI sceKernelSetThreadAtexitReport(thread_atexit_report_func_t func) {
    ThreadLifecycle::SetThreadAtexitReport(func);
}

void APS5_VABI sceKernelSetThreadDtors(thread_dtors_func_t dtors) {
    ThreadLifecycle::SetThreadDtors(dtors);
}

}

extern "C" {

void APS5_VABI _sceKernelRtldSetApplicationHeapAPI_nid_postfix(void* api[]) {
    registerApplicationHeapApi(api);
}

int APS5_VABI _sceKernelRtldThreadAtexitDecrement_nid_postfix(std::uint64_t* counter) {
    (void)counter;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _sceKernelRtldThreadAtexitIncrement_nid_postfix(std::uint64_t* counter) {
    (void)counter;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI _sceKernelSetThreadAtexitCount_nid_postfix(get_thread_atexit_count_func_t callback) {
    ThreadLifecycle::SetThreadAtexitCount(callback);
}

void APS5_VABI _sceKernelSetThreadAtexitReport_nid_postfix(thread_atexit_report_func_t callback) {
    ThreadLifecycle::SetThreadAtexitReport(callback);
}

void APS5_VABI _sceKernelSetThreadDtors_nid_postfix(thread_dtors_func_t callback) {
    ThreadLifecycle::SetThreadDtors(callback);
}

}
