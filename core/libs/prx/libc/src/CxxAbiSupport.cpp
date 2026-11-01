#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/exceptions/Runtime.hpp"
#include <cstddef>
#ifndef _UNWIND_H
#define _UNWIND_H
#endif

#include "prx/libc/include/specifics/itanium/CxxAbi.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <typeinfo>
#include <new>
#include <ios>
#include <locale>
#include <regex>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <unordered_set>
#include <memory>

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

extern "C" {

void* APS5_VABI __cxa_demangle_nid_postfix(const char* mangled, char* buf, std::size_t* len, int* status) {
    if (mangled == nullptr || (buf != nullptr && len == nullptr)) {
        if (status != nullptr) *status = -3;
        return nullptr;
    }

    int nativeStatus = 0;
    std::unique_ptr<char, decltype(&std::free)> nativeResult(
        abi::__cxa_demangle(mangled, nullptr, nullptr, &nativeStatus), &std::free);
    if (!nativeResult) {
        if (status != nullptr) *status = nativeStatus;
        return nullptr;
    }

    const std::size_t required = std::strlen(nativeResult.get()) + 1;
    char* result = buf;
    bool updateLength = false;
    try {
        if (result == nullptr) {
            result = static_cast<char*>(ApplicationHeapAllocate_nid_no_patch(required));
            updateLength = len != nullptr;
        } else if (*len < required) {
            result = static_cast<char*>(ApplicationHeapReallocate_nid_no_patch(result, required));
            updateLength = true;
        }
    } catch (const std::bad_alloc&) {
        if (status != nullptr) *status = -1;
        return nullptr;
    }

    std::memcpy(result, nativeResult.get(), required);
    if (updateLength) *len = required;
    if (status != nullptr) *status = 0;
    return result;
}

int APS5_VABI __cxa_thread_atexit_impl_nid_postfix(void (APS5_VABI *func)(void*), void* arg, void* dso) {
#ifdef _WIN32
    dso = nullptr;
#endif
    struct ThreadAtexitContext {
        void (APS5_VABI *destructor)(void*);
        void* object;
    };
    auto* context = new (std::nothrow) ThreadAtexitContext{func, arg};
    if (context == nullptr)
        return -1;
    const auto run = [](void* opaque) {
        auto* context = static_cast<ThreadAtexitContext*>(opaque);
        const auto destructor = context->destructor;
        void* object = context->object;
        delete context;
        destructor(object);
    };
#ifdef __APPLE__
    __cxxabiv1::__cxa_thread_atexit(run, context, dso);
    return 0;
#else
    const int result = __cxxabiv1::__cxa_thread_atexit(run, context, dso);
    if (result != 0)
        delete context;
    return result;
#endif
}

int APS5_VABI __cxa_thread_atexit_nid_postfix(void (APS5_VABI *func)(void*), void* arg, void* dso) {
    return __cxa_thread_atexit_impl_nid_postfix(func, arg, dso);
}

int APS5_VABI LibcInternalExtCxaThreadAtexit_nid_postfix(void (APS5_VABI *destructor)(void*), void* object, void* module_id) {
    return __cxa_thread_atexit_impl_nid_postfix(destructor, object, module_id);
}

const std::error_category* _ZSt17iostream_categoryv_nid_postfix() { return &std::iostream_category(); }

int APS5_VABI _ZSt13_Execute_onceRSt9once_flagPFiPvS1_PS1_ES1__nid_postfix(
    int* flag, int (APS5_VABI *callback)(void*, void*, void**), void* arg
) {
    static std::mutex mutex;
    static std::condition_variable changed;
    static std::unordered_set<int*> running;
    std::unique_lock lock(mutex);
    changed.wait(lock, [&] { return *flag != 0 || !running.contains(flag); });
    if (*flag != 0) return 1;
    running.insert(flag);
    lock.unlock();
    bool initialized;
    try {
        initialized = callback(nullptr, arg, nullptr) != 0;
    } catch (...) {
        lock.lock();
        running.erase(flag);
        changed.notify_all();
        throw;
    }
    lock.lock();
    if (initialized) *flag = 1;
    running.erase(flag);
    changed.notify_all();
    return initialized ? 1 : 0;
}

}
