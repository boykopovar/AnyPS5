#include "prx/libwebrtc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <thread>
#include <vector>

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

}

int main() {
    CheckRecursion();
    CheckScope();
    CheckExclusion();
    CheckNull();
    std::puts("rtc::RecursiveCriticalSection and rtc::CritScope tests passed");
    return 0;
}
