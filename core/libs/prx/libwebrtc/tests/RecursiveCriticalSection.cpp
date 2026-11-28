#include "prx/libwebrtc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr int Ebusy = 16;

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

bool HeldByAnotherThread(const WebRtcRecursiveCriticalSection& section) {
    int result = -1;
    std::thread probe([&] {
        result = pthread_mutex_trylock_nid_postfix(const_cast<PthreadMutex*>(&section.mutex));
        if (result == 0)
            pthread_mutex_unlock_nid_postfix(const_cast<PthreadMutex*>(&section.mutex));
    });
    probe.join();
    Require(result == 0 || result == Ebusy, "trylock from another thread returned neither 0 nor EBUSY");
    return result == Ebusy;
}

template<typename F>
bool ThrowsInvalidArgument(F call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void CheckRecursion() {
    WebRtcRecursiveCriticalSection section{};
    _ZN3rtc24RecursiveCriticalSectionC1Ev_nid_postfix(&section);
    Require(section.mutex != nullptr, "the constructor did not initialise the mutex");
    Require(!HeldByAnotherThread(section), "a new critical section is held");
    _ZNK3rtc24RecursiveCriticalSection5EnterEv_nid_postfix(&section);
    _ZNK3rtc24RecursiveCriticalSection5EnterEv_nid_postfix(&section);
    Require(HeldByAnotherThread(section), "a critical section entered twice is not held");
    _ZNK3rtc24RecursiveCriticalSection5LeaveEv_nid_postfix(&section);
    Require(HeldByAnotherThread(section), "one Leave after two Enters released the critical section");
    _ZNK3rtc24RecursiveCriticalSection5LeaveEv_nid_postfix(&section);
    Require(!HeldByAnotherThread(section), "two Leaves after two Enters did not release the critical section");
    _ZN3rtc24RecursiveCriticalSectionD1Ev_nid_postfix(&section);
}

void CheckScope() {
    WebRtcRecursiveCriticalSection section{};
    _ZN3rtc24RecursiveCriticalSectionC1Ev_nid_postfix(&section);
    WebRtcCritScope outer{};
    _ZN3rtc9CritScopeC1EPKNS_24RecursiveCriticalSectionE_nid_postfix(&outer, &section);
    Require(outer.section == &section, "CritScope did not store its critical section");
    Require(HeldByAnotherThread(section), "CritScope did not enter its critical section");
    WebRtcCritScope inner{};
    _ZN3rtc9CritScopeC1EPKNS_24RecursiveCriticalSectionE_nid_postfix(&inner, &section);
    _ZN3rtc9CritScopeD1Ev_nid_postfix(&inner);
    Require(HeldByAnotherThread(section), "a nested CritScope's destructor released the outer one");
    _ZN3rtc9CritScopeD1Ev_nid_postfix(&outer);
    Require(!HeldByAnotherThread(section), "CritScope's destructor did not leave its critical section");
    _ZN3rtc24RecursiveCriticalSectionD1Ev_nid_postfix(&section);
}

void CheckExclusion() {
    constexpr int Threads = 4;
    constexpr int Rounds = 20000;
    WebRtcRecursiveCriticalSection section{};
    _ZN3rtc24RecursiveCriticalSectionC1Ev_nid_postfix(&section);
    volatile long counter = 0;
    std::vector<std::thread> workers;
    for (int index = 0; index < Threads; ++index) {
        workers.emplace_back([&] {
            for (int round = 0; round < Rounds; ++round) {
                WebRtcCritScope scope{};
                _ZN3rtc9CritScopeC1EPKNS_24RecursiveCriticalSectionE_nid_postfix(&scope, &section);
                counter = counter + 1;
                _ZN3rtc9CritScopeD1Ev_nid_postfix(&scope);
            }
        });
    }
    for (auto& worker : workers)
        worker.join();
    Require(counter == static_cast<long>(Threads) * Rounds, "increments under CritScope were lost");
    _ZN3rtc24RecursiveCriticalSectionD1Ev_nid_postfix(&section);
}

void CheckNull() {
    WebRtcRecursiveCriticalSection section{};
    WebRtcCritScope scope{};
    Require(ThrowsInvalidArgument([] { _ZN3rtc24RecursiveCriticalSectionC1Ev_nid_postfix(nullptr); }), "a null constructor target did not throw");
    Require(ThrowsInvalidArgument([] { _ZN3rtc24RecursiveCriticalSectionD1Ev_nid_postfix(nullptr); }), "a null destructor target did not throw");
    Require(ThrowsInvalidArgument([] { _ZNK3rtc24RecursiveCriticalSection5EnterEv_nid_postfix(nullptr); }), "Enter on null did not throw");
    Require(ThrowsInvalidArgument([] { _ZNK3rtc24RecursiveCriticalSection5LeaveEv_nid_postfix(nullptr); }), "Leave on null did not throw");
    Require(ThrowsInvalidArgument([&] { _ZN3rtc9CritScopeC1EPKNS_24RecursiveCriticalSectionE_nid_postfix(nullptr, &section); }), "a null CritScope did not throw");
    Require(ThrowsInvalidArgument([&] { _ZN3rtc9CritScopeC1EPKNS_24RecursiveCriticalSectionE_nid_postfix(&scope, nullptr); }), "CritScope over a null critical section did not throw");
    Require(ThrowsInvalidArgument([] { _ZN3rtc9CritScopeD1Ev_nid_postfix(nullptr); }), "a null CritScope destructor did not throw");
}

class GuardedPages {
public:
    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize_ = info.dwPageSize ? static_cast<std::size_t>(info.dwPageSize) : 4096;
        base_ = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize_ * 3, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        Require(base_ != nullptr, "VirtualAlloc failed");
        DWORD oldProtect = 0;
        Require(VirtualProtect(base_ + pageSize_, pageSize_, PAGE_READONLY, &oldProtect) != 0, "VirtualProtect read-only failed");
        Require(VirtualProtect(base_ + pageSize_ * 2, pageSize_, PAGE_NOACCESS, &oldProtect) != 0, "VirtualProtect guard failed");
#else
        const long size = sysconf(_SC_PAGESIZE);
        pageSize_ = size > 0 ? static_cast<std::size_t>(size) : 4096;
        void* mapped = mmap(nullptr, pageSize_ * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED, "mmap failed");
        base_ = static_cast<unsigned char*>(mapped);
        Require(mprotect(base_ + pageSize_, pageSize_, PROT_READ) == 0, "mprotect read-only failed");
        Require(mprotect(base_ + pageSize_ * 2, pageSize_, PROT_NONE) == 0, "mprotect guard failed");
#endif
    }
    ~GuardedPages() {
        if (!base_)
            return;
#ifdef _WIN32
        VirtualFree(base_, 0, MEM_RELEASE);
#else
        munmap(base_, pageSize_ * 3);
#endif
    }
    template<typename T>
    T* ReadOnly() const {
        return reinterpret_cast<T*>(base_ + pageSize_);
    }
    template<typename T>
    T* Guard() const {
        return reinterpret_cast<T*>(base_ + pageSize_ * 2);
    }

private:
    unsigned char* base_ = nullptr;
    std::size_t pageSize_ = 4096;
};

void CheckGuarded() {
    GuardedPages pages;
    WebRtcRecursiveCriticalSection section{};
    _ZN3rtc24RecursiveCriticalSectionC1Ev_nid_postfix(&section);

    errno = 67;
    Require(ThrowsInvalidArgument([&] { _ZN3rtc24RecursiveCriticalSectionC1Ev_nid_postfix(pages.Guard<WebRtcRecursiveCriticalSection>()); }), "guarded constructor target did not throw");
    Require(ThrowsInvalidArgument([&] { _ZN3rtc24RecursiveCriticalSectionC1Ev_nid_postfix(pages.ReadOnly<WebRtcRecursiveCriticalSection>()); }), "read-only constructor target did not throw");
    Require(ThrowsInvalidArgument([&] { _ZN3rtc24RecursiveCriticalSectionD1Ev_nid_postfix(pages.Guard<WebRtcRecursiveCriticalSection>()); }), "guarded destructor target did not throw");
    Require(ThrowsInvalidArgument([&] { _ZN3rtc24RecursiveCriticalSectionD1Ev_nid_postfix(pages.ReadOnly<WebRtcRecursiveCriticalSection>()); }), "read-only destructor target did not throw");
    Require(ThrowsInvalidArgument([&] { _ZNK3rtc24RecursiveCriticalSection5EnterEv_nid_postfix(pages.Guard<WebRtcRecursiveCriticalSection>()); }), "guarded Enter target did not throw");
    Require(ThrowsInvalidArgument([&] { _ZNK3rtc24RecursiveCriticalSection5LeaveEv_nid_postfix(pages.Guard<WebRtcRecursiveCriticalSection>()); }), "guarded Leave target did not throw");
    Require(ThrowsInvalidArgument([&] { _ZN3rtc9CritScopeC1EPKNS_24RecursiveCriticalSectionE_nid_postfix(pages.Guard<WebRtcCritScope>(), &section); }), "guarded CritScope did not throw");
    Require(!HeldByAnotherThread(section), "failed guarded CritScope left section locked");
    Require(ThrowsInvalidArgument([&] { _ZN3rtc9CritScopeC1EPKNS_24RecursiveCriticalSectionE_nid_postfix(pages.ReadOnly<WebRtcCritScope>(), &section); }), "read-only CritScope did not throw");
    Require(!HeldByAnotherThread(section), "failed read-only CritScope left section locked");
    Require(ThrowsInvalidArgument([&] { _ZN3rtc9CritScopeD1Ev_nid_postfix(pages.Guard<WebRtcCritScope>()); }), "guarded CritScope destructor did not throw");
    Require(errno == 67, "guarded pointer checks clobbered errno");

    _ZN3rtc24RecursiveCriticalSectionD1Ev_nid_postfix(&section);
}

}

int main() {
    CheckRecursion();
    CheckScope();
    CheckExclusion();
    CheckNull();
    CheckGuarded();
    std::puts("rtc::RecursiveCriticalSection and rtc::CritScope tests passed");
    return 0;
}
