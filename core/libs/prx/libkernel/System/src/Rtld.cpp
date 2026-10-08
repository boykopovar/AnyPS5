#include <cstdint>
#include <cstddef>
#include <array>
#include <cstring>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Pthread/include/ThreadLifecycle.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#include <link.h>
#endif

namespace {

std::mutex rtldHeapMutex;
std::array<void*, 10> rtldHeapApi{};
std::mutex threadAtexitMutex;
std::multimap<const void*, void*> threadAtexitReferences;

const void* imageContaining(const void* address, bool reference, void*& handle, const char* caller) {
    handle = nullptr;
#ifdef _WIN32
    const DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | (reference ? 0 : GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT);
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(flags, reinterpret_cast<LPCWSTR>(address), &module))
        throw std::runtime_error(std::string(caller) + ": address outside every loaded image");
    if (reference) handle = module;
    return module;
#else
    Dl_info info{};
    link_map* image = nullptr;
    if (!dladdr1(address, &info, reinterpret_cast<void**>(&image), RTLD_DL_LINKMAP) || !image)
        throw std::runtime_error(std::string(caller) + ": address outside every loaded image");
    if (reference && image->l_name && *image->l_name) {
        handle = dlopen(image->l_name, RTLD_LAZY | RTLD_NOLOAD);
        if (!handle) throw std::runtime_error(std::string(caller) + ": cannot reference " + image->l_name);
    }
    return image;
#endif
}

int referenceThreadAtexitImage(const void* address, const char* caller) {
    void* handle = nullptr;
    const void* image = imageContaining(address, true, handle, caller);
    std::lock_guard lock(threadAtexitMutex);
    threadAtexitReferences.emplace(image, handle);
    return 0;
}

int releaseThreadAtexitImage(const void* address, const char* caller) {
    void* handle = nullptr;
    const void* image = imageContaining(address, false, handle, caller);
    {
        std::lock_guard lock(threadAtexitMutex);
        const auto found = threadAtexitReferences.find(image);
        if (found == threadAtexitReferences.end())
            throw std::runtime_error(std::string(caller) + ": image holds no thread atexit reference");
        handle = found->second;
        threadAtexitReferences.erase(found);
    }
    if (!handle) return 0;
#ifdef _WIN32
    if (!FreeLibrary(static_cast<HMODULE>(handle)))
#else
    if (dlclose(handle) != 0)
#endif
        throw std::runtime_error(std::string(caller) + ": cannot release image");
    return 0;
}

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

int APS5_VABI sceKernelRtldThreadAtexitDecrement(const void* dsoSymbol) {
    return releaseThreadAtexitImage(dsoSymbol, __func__);
}

int APS5_VABI sceKernelRtldThreadAtexitIncrement(const void* dsoSymbol) {
    return referenceThreadAtexitImage(dsoSymbol, __func__);
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

int APS5_VABI _sceKernelRtldThreadAtexitDecrement_nid_postfix(const void* dsoSymbol) {
    return releaseThreadAtexitImage(dsoSymbol, "_sceKernelRtldThreadAtexitDecrement");
}

int APS5_VABI _sceKernelRtldThreadAtexitIncrement_nid_postfix(const void* dsoSymbol) {
    return referenceThreadAtexitImage(dsoSymbol, "_sceKernelRtldThreadAtexitIncrement");
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
