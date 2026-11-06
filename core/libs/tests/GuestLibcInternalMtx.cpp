#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstdlib>
#include <thread>

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
