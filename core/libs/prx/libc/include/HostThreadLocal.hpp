#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_HOSTTHREADLOCAL_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_HOSTTHREADLOCAL_HPP

#include <memory>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <atomic>
#endif

#ifdef _WIN32
template<typename TValue, typename TTag>
class HostThreadLocalSlot {
public:
    static DWORD Get() {
        static const DWORD slot = [] {
            const DWORD created = FlsAlloc(+[](void* value) { delete static_cast<TValue*>(value); });
            published.store(created, std::memory_order_release);
            return created;
        }();
        return slot;
    }
    static TValue* Find() {
        const DWORD slot = published.load(std::memory_order_acquire);
        if (slot == FLS_OUT_OF_INDEXES) return nullptr;
        const DWORD saved = GetLastError();
        auto* value = static_cast<TValue*>(FlsGetValue(slot));
        SetLastError(saved);
        return value;
    }
private:
    static inline std::atomic<DWORD> published{FLS_OUT_OF_INDEXES};
};
template<typename TValue, typename TTag>
TValue* FindHostThreadLocal() { return HostThreadLocalSlot<TValue, TTag>::Find(); }
#endif

template<typename TValue, typename TTag>
TValue& HostThreadLocal() {
#ifdef _WIN32
    const DWORD slot = HostThreadLocalSlot<TValue, TTag>::Get();
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
