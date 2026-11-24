#include "prx/libSceLibcInternal/include/StdMutex.hpp"
#include <cstdio>
#include <cstdlib>
#include <exception>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal std::_Mutex: %s\n", message);
    std::abort();
}

struct GuestStdMutex {
    void* handle = nullptr;
};

template <typename Fn>
bool ThrowsInvalidArgument(Fn&& fn) {
    try {
        fn();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

}

int main() {
    Require(ThrowsInvalidArgument([] { _ZNSt6_MutexC1Ev_nid_postfix(nullptr); }), "C1 rejects null");
    Require(ThrowsInvalidArgument([] { _ZNSt6_MutexC2Ev_nid_postfix(nullptr); }), "C2 rejects null");
    Require(ThrowsInvalidArgument([] { _ZNSt6_MutexD2Ev_nid_postfix(nullptr); }), "D2 rejects null");
    Require(ThrowsInvalidArgument([] { _ZNSt6_Mutex5_LockEv_nid_postfix(nullptr); }), "_Lock rejects null");
    Require(ThrowsInvalidArgument([] { _ZNSt6_Mutex7_UnlockEv_nid_postfix(nullptr); }), "_Unlock rejects null");

    GuestStdMutex mutexC2{};
    _ZNSt6_MutexC2Ev_nid_postfix(&mutexC2);
    Require(mutexC2.handle != nullptr, "C2 initializes handle");
    _ZNSt6_Mutex5_LockEv_nid_postfix(&mutexC2);
    _ZNSt6_Mutex5_LockEv_nid_postfix(&mutexC2);
    _ZNSt6_Mutex7_UnlockEv_nid_postfix(&mutexC2);
    _ZNSt6_Mutex7_UnlockEv_nid_postfix(&mutexC2);
    _ZNSt6_MutexD2Ev_nid_postfix(&mutexC2);
    Require(mutexC2.handle == nullptr, "D2 clears handle");

    GuestStdMutex mutexC1{};
    _ZNSt6_MutexC1Ev_nid_postfix(&mutexC1);
    Require(mutexC1.handle != nullptr, "C1 initializes handle");
    _ZNSt6_Mutex5_LockEv_nid_postfix(&mutexC1);
    _ZNSt6_Mutex7_UnlockEv_nid_postfix(&mutexC1);
    _ZNSt6_MutexD2Ev_nid_postfix(&mutexC1);
    Require(mutexC1.handle == nullptr, "D2 clears C1 handle");

    return 0;
}
