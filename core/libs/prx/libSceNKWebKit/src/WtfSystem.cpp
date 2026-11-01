#include <atomic>
#include <condition_variable>
#include <mutex>

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include <stdexcept>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <cstddef>
#include <cstdint>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

constexpr std::uintptr_t wordLocked = 1;
constexpr std::uintptr_t wordQueueLocked = 2;
constexpr std::uintptr_t wordFlags = 3;

struct WordWaiter {
    std::mutex mutex;
    std::condition_variable condition;
    bool waiting{true};
    WordWaiter* next{nullptr};
    WordWaiter* tail{nullptr};
};

static_assert(sizeof(std::uintptr_t) == 8 && alignof(WordWaiter) >= 4);
static_assert(std::atomic_ref<std::uintptr_t>::required_alignment <= alignof(std::uintptr_t));

}  // namespace

extern "C" {

void APS5_VABI _ZN3WTF8WordLock8lockSlowEv(std::uintptr_t* self) {
    if (!self) throw std::runtime_error("WTF::WordLock: null lock");
    std::atomic_ref word(*self);
    unsigned spins = 0;
    for (;;) {
        auto value = word.load();
        if (!(value & wordLocked)) {
            if (value & wordQueueLocked) throw std::runtime_error("WTF::WordLock: queue locked without ownership");
            if (word.compare_exchange_weak(value, value | wordLocked)) return;
            continue;
        }
        if (!(value & ~wordFlags) && spins < 40) {
            ++spins;
            std::this_thread::yield();
            continue;
        }
        WordWaiter waiter;
        value = word.load();
        if ((value & wordQueueLocked) || !(value & wordLocked) || !word.compare_exchange_weak(value, value | wordQueueLocked)) {
            std::this_thread::yield();
            continue;
        }
        auto* head = reinterpret_cast<WordWaiter*>(value & ~wordFlags);
        if (head) {
            head->tail->next = &waiter;
            head->tail = &waiter;
            word.store(value);
        } else {
            waiter.tail = &waiter;
            word.store(reinterpret_cast<std::uintptr_t>(&waiter) | wordLocked);
        }
        std::unique_lock lock(waiter.mutex);
        waiter.condition.wait(lock, [&] { return !waiter.waiting; });
    }
}

void APS5_VABI _ZN3WTF8WordLock10unlockSlowEv(std::uintptr_t* self) {
    if (!self) throw std::runtime_error("WTF::WordLock: null lock");
    std::atomic_ref word(*self);
    for (;;) {
        auto value = word.load();
        if (!(value & wordLocked)) throw std::runtime_error("WTF::WordLock: unlock of an unlocked lock");
        if (value == wordLocked) {
            if (word.compare_exchange_weak(value, 0)) return;
        } else if (!(value & wordQueueLocked) && word.compare_exchange_weak(value, value | wordQueueLocked)) {
            auto* head = reinterpret_cast<WordWaiter*>(value & ~wordFlags);
            auto* next = head->next;
            if (next) next->tail = head->tail;
            word.store(reinterpret_cast<std::uintptr_t>(next));
            head->next = nullptr;
            head->tail = nullptr;
            std::lock_guard lock(head->mutex);
            head->waiting = false;
            head->condition.notify_one();
            return;
        }
        std::this_thread::yield();
    }
}

}

namespace NKWebKit {
void FillRandomBytes(void* buffer, std::size_t bytes);
}  // namespace NKWebKit

namespace {
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
