#include <atomic>
#include <chrono>
#include <cstdint>
#include <list>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"

namespace {

constexpr int SYNC_ON_ADDRESS_OK = 0;

struct AddressWaiter {
    TimedWait::Condition condition;
    bool woken = false;
};

std::mutex g_waitersLock;
std::unordered_map<std::uintptr_t, std::list<AddressWaiter*>> g_waiters;

template <class TValue>
bool IsAlignedAddress(std::uintptr_t address) {
    return address != 0 && address % alignof(TValue) == 0;
}

template <class TValue>
int WaitOnAddress(TValue* address, TValue expected, const KernelUseconds* timeout, const void* caller) {
    const auto key = reinterpret_cast<std::uintptr_t>(address);
    std::unique_lock<std::mutex> lock(g_waitersLock);
    if (std::atomic_ref<TValue>(*address).load() != expected) {
        return SYNC_ON_ADDRESS_OK;
    }

    AddressWaiter waiter;
    auto& queue = g_waiters[key];
    const auto position = queue.insert(queue.end(), &waiter);
    const auto isWoken = [&] { return waiter.woken; };
    const auto waitStart = std::chrono::steady_clock::now();
    if (timeout == nullptr) {
        waiter.condition.Wait(lock, isWoken);
    } else {
        waiter.condition.WaitUntil(lock, TimedWait::DeadlineNanos(*timeout), isWoken);
    }
    const auto waited = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - waitStart);

    const bool woken = waiter.woken;
    if (!woken) {
        queue.erase(position);
        if (queue.empty()) {
            g_waiters.erase(key);
        }
    }
    lock.unlock();
    KernelTraceWait_nid_postfix("addr", caller, static_cast<std::uint64_t>(waited.count()), !woken);
    return woken ? SYNC_ON_ADDRESS_OK : SCE_KERNEL_ERROR_ETIMEDOUT;
}

}  // namespace

extern "C" {

int APS5_VABI sceKernelSyncOnAddressWait(std::uint32_t* address, std::uint32_t expected, const KernelUseconds* timeout, const char* name) {
    (void)name;
    if (!IsAlignedAddress<std::uint32_t>(reinterpret_cast<std::uintptr_t>(address))) {
        APS5_INVALID_ARG_EX;
    }
    return WaitOnAddress(address, expected, timeout, __builtin_return_address(0));
}

int APS5_VABI sceKernelSyncOnAddressWait32(std::uint32_t* address, std::uint32_t expected, const KernelUseconds* timeout, const char* name) {
    (void)name;
    if (!IsAlignedAddress<std::uint32_t>(reinterpret_cast<std::uintptr_t>(address))) {
        APS5_INVALID_ARG_EX;
    }
    return WaitOnAddress(address, expected, timeout, __builtin_return_address(0));
}

int APS5_VABI sceKernelSyncOnAddressWait64(std::uint64_t* address, std::uint64_t expected, const KernelUseconds* timeout, const char* name) {
    (void)name;
    if (!IsAlignedAddress<std::uint64_t>(reinterpret_cast<std::uintptr_t>(address))) {
        APS5_INVALID_ARG_EX;
    }
    return WaitOnAddress(address, expected, timeout, __builtin_return_address(0));
}

int APS5_VABI sceKernelSyncOnAddressWake(void* address, std::int32_t count) {
    const auto key = reinterpret_cast<std::uintptr_t>(address);
    if (!IsAlignedAddress<std::uint32_t>(key) || count < 0) {
        APS5_INVALID_ARG_EX;
    }

    std::lock_guard<std::mutex> lock(g_waitersLock);
    const auto entry = g_waiters.find(key);
    if (entry == g_waiters.end()) {
        return SYNC_ON_ADDRESS_OK;
    }
    auto& queue = entry->second;
    for (; count > 0 && !queue.empty(); --count) {
        AddressWaiter* waiter = queue.front();
        queue.pop_front();
        waiter->woken = true;
        waiter->condition.NotifyOne();
    }
    if (queue.empty()) {
        g_waiters.erase(entry);
    }
    return SYNC_ON_ADDRESS_OK;
}

}
