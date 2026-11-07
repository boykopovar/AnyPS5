#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include <stdexcept>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <cstddef>
#include <cstdint>
#include <thread>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <limits>
#include <memory>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#endif

namespace NKWebKit {
void FillRandomBytes(void* buffer, std::size_t bytes);
}  // namespace NKWebKit

namespace {
struct HostMutex {
    std::mutex state;
    std::condition_variable available;
    std::thread::id owner;

    void lock() {
        std::unique_lock guard(state);
        available.wait(guard, [&] { return owner == std::thread::id{}; });
        owner = std::this_thread::get_id();
    }

    bool try_lock() {
        std::lock_guard guard(state);
        if (owner != std::thread::id{}) return false;
        owner = std::this_thread::get_id();
        return true;
    }

    void unlock() {
        std::lock_guard guard(state);
        if (owner != std::this_thread::get_id()) throw std::runtime_error("WTF::Mutex: unlock by a non-owner");
        owner = {};
        available.notify_one();
    }

    void RequireOwner() {
        std::lock_guard guard(state);
        if (owner != std::this_thread::get_id()) throw std::runtime_error("WTF::ThreadCondition: wait without owning the mutex");
    }
};

template<typename TValue, typename TGuest>
TValue& HostObject(TGuest* self) {
    if (!self) throw std::runtime_error("WTF threading primitive: null object");
    std::atomic_ref slot(self->impl);
    auto* current = slot.load();
    if (!current) {
        auto created = std::make_unique<TValue>();
        if (slot.compare_exchange_strong(current, created.get())) current = created.release();
    }
    return *static_cast<TValue*>(current);
}

template<typename TValue, typename TGuest>
void DestroyHostObject(TGuest* self) {
    if (!self) throw std::runtime_error("WTF threading primitive: null object");
    delete static_cast<TValue*>(std::atomic_ref(self->impl).exchange(nullptr));
}

bool WaitCondition(NKWebKit::WtfAbi::ThreadCondition* self, NKWebKit::WtfAbi::Mutex& guestMutex, double deadline) {
    if (std::isnan(deadline)) throw std::runtime_error("WTF::ThreadCondition: NaN deadline");
    if (deadline < _ZN3WTF8WallTime3nowEv().m_value) return false;
    auto& condition = HostObject<std::condition_variable_any>(self);
    auto& mutex = HostObject<HostMutex>(&guestMutex);
    mutex.RequireOwner();
    std::unique_lock lock(mutex, std::adopt_lock);
    bool signalled = false;
    try {
        if (deadline > static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
            condition.wait(lock);
            signalled = true;
        } else {
            for (;;) {
                const double now = _ZN3WTF8WallTime3nowEv().m_value;
                const double remaining = deadline - now;
                if (!(remaining > 0)) break;
                using WallDeadline = std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<double>>;
                const WallDeadline wakeTime{std::chrono::duration<double>(std::min(deadline, now + 3600.0))};
                if (condition.wait_until(lock, wakeTime) == std::cv_status::no_timeout) {
                    signalled = true;
                    break;
                }
            }
        }
    } catch (...) {
        lock.release();
        throw;
    }
    lock.release();
    return signalled;
}

bool g_configPermanentlyFrozen = false;
}  // namespace

extern "C" {
int APS5_VABI _ZN3WTF22numberOfProcessorCoresEv() {
    unsigned int count = std::thread::hardware_concurrency();
    if (count == 0) throw std::runtime_error("numberOfProcessorCores: the host reports no processor count");
    return static_cast<int>(count);
}

std::uint64_t APS5_VABI _ZN3WTF7ramSizeEv() {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return status.ullTotalPhys;
    }
#else
    const long pages = sysconf(_SC_PHYS_PAGES);
    const long pageBytes = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && pageBytes > 0) return static_cast<std::uint64_t>(pages) * static_cast<std::uint64_t>(pageBytes);
#endif
    throw std::runtime_error("ramSize: the host reports no physical memory size");
}

void APS5_VABI _ZN3WTF29cryptographicallyRandomValuesEPvm(void* buffer, std::size_t length) {
    NKWebKit::FillRandomBytes(buffer, length);
}

std::size_t APS5_VABI _ZN3WTF8pageSizeEv() {
    return 4096;
}

void APS5_VABI _ZN3WTF6Config17permanentlyFreezeEv() {
    g_configPermanentlyFrozen = true;
}

void APS5_VABI _ZN3WTF5MutexC1Ev(NKWebKit::WtfAbi::Mutex* self) {
    if (!self) throw std::runtime_error("WTF::Mutex: null object");
    self->impl = new HostMutex;
}

void APS5_VABI _ZN3WTF5MutexC2Ev(NKWebKit::WtfAbi::Mutex* self) {
    _ZN3WTF5MutexC1Ev(self);
}

void APS5_VABI _ZN3WTF5MutexD1Ev(NKWebKit::WtfAbi::Mutex* self) {
    DestroyHostObject<HostMutex>(self);
}

void APS5_VABI _ZN3WTF5MutexD2Ev(NKWebKit::WtfAbi::Mutex* self) {
    DestroyHostObject<HostMutex>(self);
}

void APS5_VABI _ZN3WTF5Mutex4lockEv(NKWebKit::WtfAbi::Mutex* self) {
    HostObject<HostMutex>(self).lock();
}

bool APS5_VABI _ZN3WTF5Mutex7tryLockEv(NKWebKit::WtfAbi::Mutex* self) {
    return HostObject<HostMutex>(self).try_lock();
}

void APS5_VABI _ZN3WTF5Mutex6unlockEv(NKWebKit::WtfAbi::Mutex* self) {
    HostObject<HostMutex>(self).unlock();
}

void APS5_VABI _ZN3WTF15ThreadCondition4waitERNS_5MutexE(NKWebKit::WtfAbi::ThreadCondition* self, NKWebKit::WtfAbi::Mutex& mutex) {
    WaitCondition(self, mutex, std::numeric_limits<double>::infinity());
}

bool APS5_VABI _ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(NKWebKit::WtfAbi::ThreadCondition* self, NKWebKit::WtfAbi::Mutex& mutex, WTF::WallTime deadline) {
    return WaitCondition(self, mutex, deadline.m_value);
}

void APS5_VABI _ZN3WTF15ThreadCondition6signalEv(NKWebKit::WtfAbi::ThreadCondition* self) {
    HostObject<std::condition_variable_any>(self).notify_one();
}

void APS5_VABI _ZN3WTF15ThreadCondition9broadcastEv(NKWebKit::WtfAbi::ThreadCondition* self) {
    HostObject<std::condition_variable_any>(self).notify_all();
}

void APS5_VABI _ZN3WTF15ThreadConditionD1Ev(NKWebKit::WtfAbi::ThreadCondition* self) {
    DestroyHostObject<std::condition_variable_any>(self);
}

void APS5_VABI _ZN3WTF15ThreadConditionD2Ev(NKWebKit::WtfAbi::ThreadCondition* self) {
    DestroyHostObject<std::condition_variable_any>(self);
}

int APS5_VABI _ZN3WTF18constantTimeMemcmpEPKvS1_m(const void* voidA, const void* voidB, std::size_t length) {
    const auto* a = static_cast<const std::uint8_t*>(voidA);
    const auto* b = static_cast<const std::uint8_t*>(voidB);
    std::uint8_t result = 0;
    for (std::size_t i = 0; i < length; ++i) {
        result |= static_cast<std::uint8_t>(a[i] ^ b[i]);
    }
    return result;
}

}
