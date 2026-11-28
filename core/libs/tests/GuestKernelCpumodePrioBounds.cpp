#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestCpumodeAndSocId() {
    *__error_nid_postfix() = 42;
    const int cpumode = sceKernelGetCpumode();
    Require(cpumode == 0);
    Require(*__error_nid_postfix() == 42);

    for (int i = 0; i < 10; ++i) {
        Require(sceKernelGetCpumode() == 0);
    }

    const int socId = sceKernelGetMainSocId();
    Require(socId == 0);
    Require(*__error_nid_postfix() == 42);

    for (int i = 0; i < 10; ++i) {
        Require(sceKernelGetMainSocId() == 0);
    }

    Require(sceKernelHasTrinityMode() == 0);
    Require(sceKernelIsTrinityMode() == 0);
    Require(sceKernelHasTrinityMode() == sceKernelIsTrinityMode());
    Require(*__error_nid_postfix() == 42);
}

void TestThreadPriorityBounds() {
    constexpr int PosixEinval = 22;
    constexpr int PosixEsrch = 3;
    constexpr int SceEinval = static_cast<int>(0x80020016u);
    constexpr int SceEsrch = static_cast<int>(0x80020003u);

    *__error_nid_postfix() = 0;
    Require(pthread_getprio_nid_postfix(nullptr) == -1);
    Require(*__error_nid_postfix() == PosixEinval);
    Require(pthread_setprio_nid_postfix(nullptr, 500) == PosixEinval);
    Require(scePthreadSetprio(nullptr, 500) == SceEinval);

    const auto invalidThread = reinterpret_cast<Pthread>(std::uintptr_t{0x12345});
    *__error_nid_postfix() = 0;
    Require(pthread_getprio_nid_postfix(invalidThread) == -1);
    Require(*__error_nid_postfix() == PosixEsrch);
    Require(pthread_setprio_nid_postfix(invalidThread, 500) == PosixEsrch);
    Require(scePthreadSetprio(invalidThread, 500) == SceEsrch);

    const Pthread self = scePthreadSelf();
    Require(self != nullptr);

    const int originalPrio = pthread_getprio_nid_postfix(self);
    Require(originalPrio >= 256 && originalPrio <= 767);

    Require(scePthreadSetprio(self, 256) == 0);
    Require(pthread_getprio_nid_postfix(self) == 256);

    Require(scePthreadSetprio(self, 767) == 0);
    Require(pthread_getprio_nid_postfix(self) == 767);

    Require(pthread_setprio_nid_postfix(self, 500) == 0);
    Require(pthread_getprio_nid_postfix(self) == 500);

    Require(scePthreadSetprio(self, 896) == 0);
    Require(pthread_getprio_nid_postfix(self) == 896);

    Require(scePthreadSetprio(self, 959) == 0);
    Require(pthread_getprio_nid_postfix(self) == 959);

    Require(scePthreadSetprio(self, 65792) == 0);
    Require(pthread_getprio_nid_postfix(self) == 65792);

    Require(scePthreadSetprio(self, -65280) == 0);
    Require(pthread_getprio_nid_postfix(self) == -65280);

    const int invalidPrios[] = {0, 100, 255, 768, 895, 960, 1000, 65535};
    for (int invalidPrio : invalidPrios) {
        *__error_nid_postfix() = 0;
        Require(scePthreadSetprio(self, invalidPrio) == SceEinval);
        Require(*__error_nid_postfix() == PosixEinval);

        *__error_nid_postfix() = 0;
        Require(pthread_setprio_nid_postfix(self, invalidPrio) == PosixEinval);
        Require(*__error_nid_postfix() == PosixEinval);
    }

    Require(scePthreadSetprio(self, originalPrio) == 0);
    Require(pthread_getprio_nid_postfix(self) == originalPrio);
}

void TestPriorityCeilingBounds() {
    constexpr int PosixEinval = 22;
    constexpr int SceEinval = static_cast<int>(0x80020016u);
    constexpr int ProtocolProtect = 2;

    int ceilingOut = -1;
    Require(pthread_mutexattr_setprioceiling_nid_postfix(nullptr, 300) == PosixEinval);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(nullptr, &ceilingOut) == PosixEinval);
    Require(scePthreadMutexattrSetprioceiling(nullptr, 300) == SceEinval);
    Require(scePthreadMutexattrGetprioceiling(nullptr, &ceilingOut) == SceEinval);

    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);

    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 300) == PosixEinval);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceilingOut) == PosixEinval);
    Require(scePthreadMutexattrSetprioceiling(&attr, 300) == SceEinval);
    Require(scePthreadMutexattrGetprioceiling(&attr, &ceilingOut) == SceEinval);

    Require(pthread_mutexattr_setprotocol_nid_postfix(&attr, ProtocolProtect) == 0);

    const int badCeilings[] = {-1, 0, 100, 255, 768, 896, 1000};
    for (int badCeiling : badCeilings) {
        Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, badCeiling) == PosixEinval);
        Require(scePthreadMutexattrSetprioceiling(&attr, badCeiling) == SceEinval);
    }

    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 256) == 0);
    Require(pthread_mutexattr_getprioceiling_nid_postfix(&attr, &ceilingOut) == 0 && ceilingOut == 256);

    Require(scePthreadMutexattrSetprioceiling(&attr, 767) == 0);
    Require(scePthreadMutexattrGetprioceiling(&attr, &ceilingOut) == 0 && ceilingOut == 767);

    Require(pthread_mutexattr_setprioceiling_nid_postfix(&attr, 500) == 0);

    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, &attr) == 0);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);

    Require(pthread_mutex_getprioceiling_nid_postfix(&mutex, &ceilingOut) == 0 && ceilingOut == 500);
    Require(scePthreadMutexGetprioceiling(&mutex, &ceilingOut) == 0 && ceilingOut == 500);

    int oldCeiling = -1;
    Require(pthread_mutex_setprioceiling_nid_postfix(&mutex, 600, &oldCeiling) == 0 && oldCeiling == 500);
    Require(pthread_mutex_getprioceiling_nid_postfix(&mutex, &ceilingOut) == 0 && ceilingOut == 600);

    Require(scePthreadMutexSetprioceiling(&mutex, 700, &oldCeiling) == 0 && oldCeiling == 600);
    Require(scePthreadMutexGetprioceiling(&mutex, &ceilingOut) == 0 && ceilingOut == 700);

    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);

    Require(pthread_mutex_getprioceiling_nid_postfix(&mutex, &ceilingOut) == PosixEinval);
    Require(scePthreadMutexGetprioceiling(&mutex, &ceilingOut) == SceEinval);
    Require(pthread_mutex_setprioceiling_nid_postfix(&mutex, 300, &oldCeiling) == PosixEinval);
    Require(scePthreadMutexSetprioceiling(&mutex, 300, &oldCeiling) == SceEinval);
}

} // namespace

int main() {
    TestCpumodeAndSocId();
    TestThreadPriorityBounds();
    TestPriorityCeilingBounds();
    return 0;
}
