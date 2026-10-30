#include "prx/libSceAddressSanitizer/src/StackBounds.hpp"

#if defined(_WIN32)
#include <windows.h>

namespace AsanRuntime {

bool CurrentStackBounds_nid_no_patch(std::uintptr_t& bottom, std::uintptr_t& top) {
    using GetLimits = void(WINAPI*)(PULONG_PTR, PULONG_PTR);
    static const auto getLimits = reinterpret_cast<GetLimits>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetCurrentThreadStackLimits")));
    if (!getLimits) {
        return false;
    }
    ULONG_PTR low = 0;
    ULONG_PTR high = 0;
    getLimits(&low, &high);
    bottom = low;
    top = high;
    return top > bottom;
}

}  // namespace AsanRuntime
#elif defined(__linux__)
#include <pthread.h>

namespace AsanRuntime {

bool CurrentStackBounds_nid_no_patch(std::uintptr_t& bottom, std::uintptr_t& top) {
    pthread_attr_t attributes;
    if (pthread_getattr_np(pthread_self(), &attributes) != 0) {
        return false;
    }
    void* address = nullptr;
    std::size_t size = 0;
    const bool ok = pthread_attr_getstack(&attributes, &address, &size) == 0;
    pthread_attr_destroy(&attributes);
    if (!ok || size == 0) {
        return false;
    }
    bottom = reinterpret_cast<std::uintptr_t>(address);
    top = bottom + size;
    return true;
}

}  // namespace AsanRuntime
#else
namespace AsanRuntime {

bool CurrentStackBounds_nid_no_patch(std::uintptr_t&, std::uintptr_t&) {
    return false;
}

}  // namespace AsanRuntime
#endif
