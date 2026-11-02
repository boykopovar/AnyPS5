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
#include <condition_variable>
#include <limits>
#include <list>
#include <mutex>
#include <random>

#ifdef _WIN32
#include <windows.h>
#endif

namespace NKWebKit {
void FillRandomBytes(void* buffer, std::size_t bytes);
}  // namespace NKWebKit

namespace {
bool g_configPermanentlyFrozen = false;

using NKWebKit::WtfAbi::ParkResult;
using NKWebKit::WtfAbi::UnparkResult;

struct ParkedThread {
    std::condition_variable condition;
    const void* address;
    std::thread::id thread{std::this_thread::get_id()};
    bool unparked{false};
    std::intptr_t token{0};
};

struct ParkingState {
    std::mutex mutex;
    std::list<ParkedThread*> queue;
    std::chrono::steady_clock::time_point nextFairTime;
    std::minstd_rand random;

    bool Dequeued() {
        const auto now = std::chrono::steady_clock::now();
        if (now <= nextFairTime) return false;
        const auto delay = std::chrono::duration<double, std::milli>(std::generate_canonical<double, 32>(random));
        nextFairTime = now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(delay);
        return true;
    }
};

ParkingState& Parking() {
    static auto* state = new ParkingState;
    return *state;
}

void CheckAddress(const void* address) {
    if (!address) throw std::runtime_error("WTF::ParkingLot: null address");
}

template<typename Validation, typename BeforeSleep>
ParkResult Park(const void* address, Validation validation, BeforeSleep beforeSleep, const WTF::TimeWithDynamicClockType& timeout) {
    CheckAddress(address);
    ParkedThread waiter{{}, address};
    std::unique_lock lock(Parking().mutex);
    for (const auto* parked : Parking().queue) {
        if (parked->thread == waiter.thread) throw std::runtime_error("WTF::ParkingLot: recursive park");
    }
    if (!validation()) return {};
    Parking().queue.push_back(&waiter);
    const auto position = std::prev(Parking().queue.end());
    const auto remove = [&] {
        if (!waiter.unparked) {
            Parking().queue.erase(position);
            Parking().Dequeued();
        }
    };
    lock.unlock();
    try {
        beforeSleep();
        lock.lock();
        while (!waiter.unparked) {
            const double now = _ZNK3WTF24TimeWithDynamicClockType16nowWithSameClockEv(&timeout).m_value;
            const double remaining = timeout.m_value - now;
            if (!(remaining > 0)) break;
            if (remaining == std::numeric_limits<double>::infinity()) waiter.condition.wait(lock);
            else if (timeout.m_type == WTF::ClockType::Wall) {
                using WallDeadline = std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<double>>;
                const WallDeadline wakeTime{std::chrono::duration<double>(std::min(timeout.m_value, now + 3600.0))};
                waiter.condition.wait_until(lock, wakeTime);
            } else waiter.condition.wait_for(lock, std::chrono::duration<double>(std::min(remaining, 3600.0)));
        }
    } catch (...) {
        if (!lock.owns_lock()) lock.lock();
        remove();
        throw;
    }
    remove();
    return {waiter.unparked, waiter.token};
}

template<typename Callback>
void UnparkOne(const void* address, Callback callback) {
    CheckAddress(address);
    std::lock_guard lock(Parking().mutex);
    const auto matches = [address](const ParkedThread* waiter) { return waiter->address == address; };
    const auto position = std::find_if(Parking().queue.begin(), Parking().queue.end(), matches);
    ParkedThread* waiter = nullptr;
    UnparkResult result;
    if (position != Parking().queue.end()) {
        waiter = *position;
        Parking().queue.erase(position);
        result.didUnparkThread = true;
        result.mayHaveMoreThreads = std::any_of(Parking().queue.begin(), Parking().queue.end(), matches);
        result.timeToBeFair = Parking().Dequeued();
        waiter->unparked = true;
    }
    try {
        const auto token = callback(result);
        if (waiter) waiter->token = token;
    } catch (...) {
        if (waiter) waiter->condition.notify_one();
        throw;
    }
    if (waiter) waiter->condition.notify_one();
}

void LockSlow(std::uint8_t* self) {
    CheckAddress(self);
    std::atomic_ref byte(*self);
    unsigned spins = 0;
    for (;;) {
        auto value = byte.load();
        if (!(value & 1)) {
            if (byte.compare_exchange_weak(value, value | 1)) return;
            continue;
        }
        if (!(value & 2) && spins < 40) {
            ++spins;
            std::this_thread::yield();
            continue;
        }
        if (!(value & 2)) {
            if (!byte.compare_exchange_weak(value, value | 2)) continue;
            value |= 2;
        }
        const auto result = Park(self, [&] { return byte.load() == value; }, [] {},
                                 {std::numeric_limits<double>::infinity(), WTF::ClockType::Monotonic});
        if (result.wasUnparked && result.token == 1) return;
    }
}

void UnlockSlow(std::uint8_t* self, bool fair) {
    CheckAddress(self);
    std::atomic_ref byte(*self);
    for (;;) {
        auto value = byte.load();
        if (!(value & 1)) throw std::runtime_error("WTF::Lock: unlock of an unlocked lock");
        if (!(value & 2)) {
            if (byte.compare_exchange_weak(value, value & ~1)) return;
            continue;
        }
        UnparkOne(self, [&](UnparkResult result) -> std::intptr_t {
            if (result.didUnparkThread && (fair || result.timeToBeFair)) return 1;
            auto current = byte.load();
            while (!byte.compare_exchange_weak(current, (current & ~3) | (result.mayHaveMoreThreads ? 2 : 0))) {}
            return 0;
        });
        return;
    }
}

struct LockAdapter {
    std::uint8_t* storage;
    void lock() { LockSlow(storage); }
    void unlock() { UnlockSlow(storage, false); }
};

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

extern "C" {

NKWebKit::WtfAbi::ParkResult APS5_VABI _ZN3WTF10ParkingLot21parkConditionallyImplEPKvRKNS_12ScopedLambdaIFbvEEERKNS3_IFvvEEERKNS_24TimeWithDynamicClockTypeE(const void* address, const NKWebKit::WtfAbi::ScopedLambda<bool()>& validation, const NKWebKit::WtfAbi::ScopedLambda<void()>& beforeSleep, const WTF::TimeWithDynamicClockType& timeout) {
    return Park(address, [&] { return validation.impl(validation.arg); }, [&] { beforeSleep.impl(beforeSleep.arg); }, timeout);
}

void APS5_VABI _ZN3WTF10ParkingLot13unparkOneImplEPKvRKNS_12ScopedLambdaIFlNS0_12UnparkResultEEEE(const void* address, const NKWebKit::WtfAbi::ScopedLambda<std::intptr_t(NKWebKit::WtfAbi::UnparkResult)>& callback) {
    UnparkOne(address, [&](UnparkResult result) { return callback.impl(callback.arg, result); });
}

NKWebKit::WtfAbi::UnparkResult APS5_VABI _ZN3WTF10ParkingLot9unparkOneEPKv(const void* address) {
    UnparkResult result;
    UnparkOne(address, [&](UnparkResult value) { result = value; return 0; });
    result.timeToBeFair = false;
    return result;
}

unsigned APS5_VABI _ZN3WTF10ParkingLot11unparkCountEPKvj(const void* address, unsigned count) {
    if (!count) return 0;
    CheckAddress(address);
    std::lock_guard lock(Parking().mutex);
    unsigned removed = 0;
    for (auto position = Parking().queue.begin(); position != Parking().queue.end() && removed < count;) {
        auto* waiter = *position;
        if (waiter->address != address) {
            ++position;
            continue;
        }
        position = Parking().queue.erase(position);
        waiter->unparked = true;
        waiter->condition.notify_one();
        ++removed;
    }
    if (removed) Parking().Dequeued();
    return removed;
}

void APS5_VABI _ZN3WTF10ParkingLot9unparkAllEPKv(const void* address) {
    _ZN3WTF10ParkingLot11unparkCountEPKvj(address, std::numeric_limits<unsigned>::max());
}

void APS5_VABI _ZN3WTF4Lock8lockSlowEv(std::uint8_t* self) {
    LockSlow(self);
}

void APS5_VABI _ZN3WTF4Lock10unlockSlowEv(std::uint8_t* self) {
    UnlockSlow(self, false);
}

void APS5_VABI _ZN3WTF4Lock13safepointSlowEv(std::uint8_t* self) {
    UnlockSlow(self, true);
    LockSlow(self);
}

}

extern "C" {

void APS5_VABI _ZN3WTF15BinarySemaphore6signalEv(NKWebKit::WtfAbi::BinarySemaphore* self) {
    CheckAddress(self);
    LockAdapter adapter{&self->lock};
    std::unique_lock lock(adapter);
    self->isSet = true;
    std::atomic_ref hasWaiters(self->hasWaiters);
    if (hasWaiters.load()) {
        UnparkOne(&self->hasWaiters, [&](UnparkResult result) -> std::intptr_t {
            if (!result.mayHaveMoreThreads) hasWaiters.store(false);
            return 0;
        });
    }
}

bool APS5_VABI _ZN3WTF15BinarySemaphore9waitUntilERKNS_24TimeWithDynamicClockTypeE(NKWebKit::WtfAbi::BinarySemaphore* self, const WTF::TimeWithDynamicClockType& timeout) {
    CheckAddress(self);
    LockAdapter adapter{&self->lock};
    std::unique_lock lock(adapter);
    while (!self->isSet) {
        bool notified = false;
        if (timeout.m_value < _ZNK3WTF24TimeWithDynamicClockType16nowWithSameClockEv(&timeout).m_value) {
            lock.unlock();
        } else {
            notified = Park(&self->hasWaiters, [&] {
                std::atomic_ref(self->hasWaiters).store(true);
                return true;
            }, [&] { lock.unlock(); }, timeout).wasUnparked;
        }
        lock.lock();
        if (!notified) break;
    }
    const bool satisfied = self->isSet;
    if (satisfied) self->isSet = false;
    return satisfied;
}

}
