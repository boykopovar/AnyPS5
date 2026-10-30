#include "ShadowAddressSpace.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <atomic>
#include <bit>
#include <cstdint>
#include <mutex>

namespace {

std::mutex initMutex;
bool initialized = false;
std::atomic<bool> active{false};
std::uintptr_t pageSize = 0;

LONG CALLBACK shadowExceptionHandler(PEXCEPTION_POINTERS info) noexcept {
    if (!active.load(std::memory_order_acquire) || info == nullptr || info->ExceptionRecord == nullptr) return EXCEPTION_CONTINUE_SEARCH;
    const auto* record = info->ExceptionRecord;
    if (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || (record->ExceptionFlags & EXCEPTION_NONCONTINUABLE) != 0 || record->NumberParameters < 2) return EXCEPTION_CONTINUE_SEARCH;
    const auto operation = record->ExceptionInformation[0];
    if (operation != 0 && operation != 1) return EXCEPTION_CONTINUE_SEARCH;
    const auto address = static_cast<std::uintptr_t>(record->ExceptionInformation[1]);
    if (address < AsanRuntime::ShadowBase || address >= AsanRuntime::ShadowBase + AsanRuntime::ShadowSize) return EXCEPTION_CONTINUE_SEARCH;
    auto* page = reinterpret_cast<void*>(address & ~(pageSize - 1));
    MEMORY_BASIC_INFORMATION region{};
    if (VirtualQuery(page, &region, sizeof(region)) != sizeof(region) || region.AllocationBase != reinterpret_cast<void*>(AsanRuntime::ShadowBase)) return EXCEPTION_CONTINUE_SEARCH;
    if (region.State == MEM_RESERVE) {
        if (VirtualAlloc(page, pageSize, MEM_COMMIT, PAGE_READWRITE) != page) return EXCEPTION_CONTINUE_SEARCH;
        if (VirtualQuery(page, &region, sizeof(region)) != sizeof(region)) return EXCEPTION_CONTINUE_SEARCH;
    }
    if (region.State != MEM_COMMIT || (region.Protect & PAGE_GUARD) != 0 || (region.Protect & 0xffu) != PAGE_READWRITE) return EXCEPTION_CONTINUE_SEARCH;
    return EXCEPTION_CONTINUE_EXECUTION;
}

}

namespace AsanRuntime {

extern "C" bool InitializeShadow_nid_no_patch() noexcept {
    if (active.load(std::memory_order_acquire)) return true;
    try {
        std::lock_guard lock(initMutex);
        if (initialized) return true;
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        if (info.dwPageSize == 0 || !std::has_single_bit(info.dwPageSize) || !active.is_lock_free()) return false;
        if (reinterpret_cast<std::uintptr_t>(info.lpMaximumApplicationAddress) < ShadowBase + ShadowSize - 1) return false;
        pageSize = info.dwPageSize;
        auto* requested = reinterpret_cast<void*>(ShadowBase);
        auto* base = VirtualAlloc(requested, ShadowSize, MEM_RESERVE, PAGE_NOACCESS);
        if (base == nullptr) return false;
        if (base != requested) {
            VirtualFree(base, 0, MEM_RELEASE);
            return false;
        }
        auto* handler = AddVectoredExceptionHandler(1, shadowExceptionHandler);
        if (handler == nullptr) {
            VirtualFree(base, 0, MEM_RELEASE);
            return false;
        }
        HMODULE module = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&InitializeShadow_nid_no_patch), &module)) {
            RemoveVectoredExceptionHandler(handler);
            VirtualFree(base, 0, MEM_RELEASE);
            return false;
        }
        active.store(true, std::memory_order_release);
        initialized = true;
        return true;
    } catch (...) {
        return false;
    }
}

}
#endif
