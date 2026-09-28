#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_HOSTTHREADLOCAL_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_HOSTTHREADLOCAL_HPP

#include <memory>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

template<typename TValue, typename TTag>
TValue& HostThreadLocal() {
#ifdef _WIN32
    static const DWORD slot = FlsAlloc(+[](void* value) { delete static_cast<TValue*>(value); });
    if (slot == FLS_OUT_OF_INDEXES) throw std::runtime_error("host thread-local slot allocation failed");
    auto* value = static_cast<TValue*>(FlsGetValue(slot));
    if (value == nullptr) {
        auto created = std::make_unique<TValue>();
        if (!FlsSetValue(slot, created.get())) throw std::runtime_error("host thread-local value registration failed");
        value = created.release();
    }
    return *value;
#else
    static thread_local TValue value;
    return value;
#endif
}

#endif
