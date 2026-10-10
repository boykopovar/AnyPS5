#pragma once

#ifdef _WIN32
#include <windows.h>
#include <memory>
#include <stdexcept>
template<class T>
class WindowsThreadLocal {
public:
    static T& Get() {
        if (slot == FLS_OUT_OF_INDEXES) throw std::runtime_error("Cannot allocate native thread state slot");
        if (auto* value = static_cast<T*>(FlsGetValue(slot))) return *value;
        auto value = std::make_unique<T>();
        if (!FlsSetValue(slot, value.get())) throw std::runtime_error("Cannot bind native thread state");
        return *value.release();
    }

private:
    static void WINAPI Destroy(void* value) {
        FlsSetValue(slot, value);
        delete static_cast<T*>(value);
        FlsSetValue(slot, nullptr);
    }
    inline static const DWORD slot = FlsAlloc(Destroy);
};
#endif
