#include "prx/libSceLibcInternal/libSceLibcInternal.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <cstdint>
#ifndef _WIN32
#include <time.h>
#else
#include <windows.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal mtx: %s\n", message);
    std::abort();
}

template<typename TCall>
int OnOtherThread(TCall call, PthreadMutex* mutex) {
    int result = -1;
    std::thread other([&] { result = call(mutex); });
    other.join();
    return result;
}

std::uint64_t DeadlineMilliseconds() {
#ifdef _WIN32
    return ::GetTickCount64();
#else
    struct timespec now{};
    Require(::clock_gettime(CLOCK_MONOTONIC, &now) == 0, "CLOCK_MONOTONIC sample succeeds");
    return static_cast<std::uint64_t>(now.tv_sec) * 1000ULL + static_cast<std::uint64_t>(now.tv_nsec) / 1000000ULL;
#endif
}

void CheckAtomicWaitDeadline() {
    Require(__std_atomic_wait_get_deadline_nid_postfix(UINT64_MAX) == UINT64_MAX, "no-deadline sentinel passes through");
    const std::uint64_t timeouts[] = {0, 1, 25, UINT64_MAX - 1};
    for (const auto timeout : timeouts) {
        const auto before = DeadlineMilliseconds();
        const auto deadline = __std_atomic_wait_get_deadline_nid_postfix(timeout);
        const auto after = DeadlineMilliseconds();
        const auto sampledAt = deadline - timeout;
        Require(sampledAt - before <= after - before, "deadline uses a monotonic millisecond sample and unsigned addition");
    }
    const auto first = __std_atomic_wait_get_deadline_nid_postfix(0);
    const auto second = __std_atomic_wait_get_deadline_nid_postfix(0);
    Require(second >= first, "zero-timeout deadlines are monotonic");
}

int TryLock(PthreadMutex* mutex) {
    const int result = pthread_mutex_trylock_nid_postfix(mutex);
    if (result == 0) pthread_mutex_unlock_nid_postfix(mutex);
    return result;
}

int LockAndUnlock(PthreadMutex* mutex) {
    if (_Mtxlock_nid_postfix(mutex) != 0) return 1;
    return _Mtxunlock_nid_postfix(mutex);
}

}

int main() {
    CheckAtomicWaitDeadline();
    PthreadMutex mutex = nullptr;
    Require(_Mtxinit_nid_postfix(&mutex, "File") == 0, "init with a name succeeds");
    Require(mutex != nullptr, "init stores a mutex");
    Require(_Mtxlock_nid_postfix(&mutex) == 0, "first lock succeeds");
    Require(_Mtxlock_nid_postfix(&mutex) == 0, "the mutex is recursive");
    Require(OnOtherThread(TryLock, &mutex) != 0, "another thread cannot take a held mutex");
    Require(OnOtherThread(_Mtxunlock_nid_postfix, &mutex) == 1, "another thread cannot unlock it");
    Require(_Mtxunlock_nid_postfix(&mutex) == 0, "first unlock succeeds");
    Require(OnOtherThread(TryLock, &mutex) != 0, "the mutex stays held until every lock is released");
    Require(_Mtxunlock_nid_postfix(&mutex) == 0, "second unlock succeeds");
    Require(_Mtxunlock_nid_postfix(&mutex) == 1, "unlocking a released mutex fails");
    Require(OnOtherThread(LockAndUnlock, &mutex) == 0, "another thread takes the released mutex");
    Require(_Mtxdst_nid_postfix(&mutex) == 0, "destroy succeeds");
    Require(_Mtxdst_nid_postfix(&mutex) == 1, "destroying twice fails");
    Require(_Mtxlock_nid_postfix(&mutex) == 1, "locking a destroyed mutex fails");

    PthreadMutex unnamed = nullptr;
    Require(_Mtxinit_nid_postfix(&unnamed, nullptr) == 0, "init without a name succeeds");
    Require(LockAndUnlock(&unnamed) == 0, "an unnamed mutex locks");
    Require(_Mtxdst_nid_postfix(&unnamed) == 0, "an unnamed mutex is destroyed");

    PthreadMutex longName = nullptr;
    Require(_Mtxinit_nid_postfix(&longName, "ANameLongerThanTheThirtyTwoByteLabel") == 0, "a long name is truncated");
    Require(_Mtxdst_nid_postfix(&longName) == 0, "a mutex with a long name is destroyed");

    PthreadMutex zeroed = nullptr;
    Require(_Mtxunlock_nid_postfix(&zeroed) == 1, "unlocking a zeroed mutex fails");
    Require(LockAndUnlock(&zeroed) == 0, "a zeroed mutex is initialized on first lock");
    Require(_Mtxdst_nid_postfix(&zeroed) == 0, "an initialized zeroed mutex is destroyed");

    PthreadMutex untouched = nullptr;
    Require(_Mtxdst_nid_postfix(&untouched) == 0, "destroying a zeroed mutex succeeds");
    return 0;
}
