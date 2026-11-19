#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Rwlock.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <thread>

namespace {

constexpr int PosixEperm = 1;
constexpr int PosixEdeadlk = 11;
constexpr int PosixEfault = 14;
constexpr int PosixEbusy = 16;
constexpr int PosixEinval = 22;
constexpr int PosixEagain = 35;
constexpr int PosixEtimedout = 60;

constexpr int SceOk = 0;
constexpr int SceKernelErrorEperm = 0x80020001;
constexpr int SceKernelErrorEdeadlk = 0x8002000B;
constexpr int SceKernelErrorEfault = 0x8002000E;
constexpr int SceKernelErrorEbusy = 0x80020010;
constexpr int SceKernelErrorEinval = 0x80020016;
constexpr int SceKernelErrorEtimedout = 0x8002003C;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestRwlockattrTypesAndValidation() {
    PthreadRwlockattr attr = nullptr;
    Require(pthread_rwlockattr_init_nid_postfix(&attr) == 0);

    int type = -1;
    Require(pthread_rwlockattr_gettype_np_nid_postfix(&attr, &type) == 0 && type == 0);
    Require(scePthreadRwlockattrGettype(&attr, &type) == SceOk && type == 0);

    Require(pthread_rwlockattr_settype_np_nid_postfix(&attr, 1) == 0);
    Require(pthread_rwlockattr_gettype_np_nid_postfix(&attr, &type) == 0 && type == 1);
    Require(scePthreadRwlockattrGettype(&attr, &type) == SceOk && type == 1);

    Require(pthread_rwlockattr_settype_np_nid_postfix(&attr, 2) == 0);
    Require(pthread_rwlockattr_gettype_np_nid_postfix(&attr, &type) == 0 && type == 2);
    Require(scePthreadRwlockattrGettype(&attr, &type) == SceOk && type == 2);

    Require(scePthreadRwlockattrSettype(&attr, 1) == SceOk);
    Require(pthread_rwlockattr_gettype_np_nid_postfix(&attr, &type) == 0 && type == 1);

    Require(scePthreadRwlockattrSettype(&attr, 2) == SceOk);
    Require(pthread_rwlockattr_gettype_np_nid_postfix(&attr, &type) == 0 && type == 2);

    const int invalidTypes[] = {-100, -1, 0, 3, 4, 10, 100, 9999};
    for (int invalid : invalidTypes) {
        Require(pthread_rwlockattr_settype_np_nid_postfix(&attr, invalid) == PosixEinval);
        Require(scePthreadRwlockattrSettype(&attr, invalid) == SceKernelErrorEinval);
        Require(pthread_rwlockattr_gettype_np_nid_postfix(&attr, &type) == 0 && type == 2);
    }

    Require(pthread_rwlockattr_settype_np_nid_postfix(nullptr, 1) == PosixEinval);
    Require(pthread_rwlockattr_settype_np_nid_postfix(nullptr, 0) == PosixEinval);
    Require(scePthreadRwlockattrSettype(nullptr, 1) == SceKernelErrorEinval);
    Require(scePthreadRwlockattrSettype(nullptr, 0) == SceKernelErrorEinval);

    Require(pthread_rwlockattr_gettype_np_nid_postfix(&attr, nullptr) == PosixEfault);
    Require(scePthreadRwlockattrGettype(&attr, nullptr) == SceKernelErrorEfault);

    int pshared = -1;
    Require(pthread_rwlockattr_getpshared_nid_postfix(&attr, &pshared) == 0 && pshared == 0);
    Require(scePthreadRwlockattrGetpshared(&attr, &pshared) == SceOk && pshared == 0);

    Require(pthread_rwlockattr_setpshared_nid_postfix(&attr, 0) == 0);
    Require(scePthreadRwlockattrSetpshared(&attr, 0) == SceOk);

    Require(pthread_rwlockattr_setpshared_nid_postfix(&attr, 1) == PosixEinval);
    Require(pthread_rwlockattr_setpshared_nid_postfix(&attr, -1) == PosixEinval);
    Require(scePthreadRwlockattrSetpshared(&attr, 1) == SceKernelErrorEinval);
    Require(scePthreadRwlockattrSetpshared(&attr, -1) == SceKernelErrorEinval);

    Require(pthread_rwlockattr_setpshared_nid_postfix(nullptr, 1) == PosixEinval);
    Require(pthread_rwlockattr_setpshared_nid_postfix(nullptr, 2) == PosixEinval);

    Require(pthread_rwlockattr_getpshared_nid_postfix(&attr, nullptr) == PosixEfault);
    Require(scePthreadRwlockattrGetpshared(&attr, nullptr) == SceKernelErrorEfault);

    Require(pthread_rwlockattr_destroy_nid_postfix(&attr) == 0);
    Require(pthread_rwlockattr_settype_np_nid_postfix(&attr, 1) == PosixEinval);
    Require(scePthreadRwlockattrSettype(&attr, 1) == SceKernelErrorEinval);
}

void TestReadWriteLockingPrecedence() {
    PthreadRwlock rwlock = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&rwlock, nullptr) == 0);

    Require(pthread_rwlock_rdlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_rdlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_tryrdlock_nid_postfix(&rwlock) == 0);

    Require(pthread_rwlock_trywrlock_nid_postfix(&rwlock) == PosixEbusy);
    Require(scePthreadRwlockTrywrlock(&rwlock) == SceKernelErrorEbusy);

    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);

    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == PosixEperm);
    Require(scePthreadRwlockUnlock(&rwlock) == SceKernelErrorEperm);

    Require(pthread_rwlock_wrlock_nid_postfix(&rwlock) == 0);

    Require(pthread_rwlock_rdlock_nid_postfix(&rwlock) == PosixEdeadlk);
    Require(pthread_rwlock_wrlock_nid_postfix(&rwlock) == PosixEdeadlk);
    Require(scePthreadRwlockRdlock(&rwlock) == SceKernelErrorEdeadlk);
    Require(scePthreadRwlockWrlock(&rwlock) == SceKernelErrorEdeadlk);

    Require(pthread_rwlock_tryrdlock_nid_postfix(&rwlock) == PosixEbusy);
    Require(pthread_rwlock_trywrlock_nid_postfix(&rwlock) == PosixEbusy);
    Require(scePthreadRwlockTryrdlock(&rwlock) == SceKernelErrorEbusy);
    Require(scePthreadRwlockTrywrlock(&rwlock) == SceKernelErrorEbusy);

    Require(pthread_rwlock_timedrdlock_nid_postfix(&rwlock, nullptr) == PosixEdeadlk);
    Require(pthread_rwlock_timedwrlock_nid_postfix(&rwlock, nullptr) == PosixEdeadlk);

    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == PosixEperm);
    Require(scePthreadRwlockUnlock(&rwlock) == SceKernelErrorEperm);

    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == PosixEinval);
    Require(scePthreadRwlockDestroy(&rwlock) == SceKernelErrorEinval);

    Require(pthread_rwlock_rdlock_nid_postfix(&rwlock) == PosixEinval);
    Require(pthread_rwlock_wrlock_nid_postfix(&rwlock) == PosixEinval);
    Require(pthread_rwlock_tryrdlock_nid_postfix(&rwlock) == PosixEinval);
    Require(pthread_rwlock_trywrlock_nid_postfix(&rwlock) == PosixEinval);
    Require(pthread_rwlock_timedrdlock_nid_postfix(&rwlock, &validPast) == PosixEinval);
    Require(pthread_rwlock_timedwrlock_nid_postfix(&rwlock, &validPast) == PosixEinval);
    Require(pthread_rwlock_reltimedrdlock_np_nid_postfix(&rwlock, 0) == PosixEinval);
    Require(pthread_rwlock_reltimedwrlock_np_nid_postfix(&rwlock, 0) == PosixEinval);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == PosixEinval);

    Require(scePthreadRwlockRdlock(&rwlock) == SceKernelErrorEinval);
    Require(scePthreadRwlockWrlock(&rwlock) == SceKernelErrorEinval);
    Require(scePthreadRwlockTryrdlock(&rwlock) == SceKernelErrorEinval);
    Require(scePthreadRwlockTrywrlock(&rwlock) == SceKernelErrorEinval);
    Require(scePthreadRwlockTimedrdlock(&rwlock, 0) == SceKernelErrorEinval);
    Require(scePthreadRwlockTimedwrlock(&rwlock, 0) == SceKernelErrorEinval);
    Require(scePthreadRwlockUnlock(&rwlock) == SceKernelErrorEinval);
}

struct ContentionState {
    PthreadRwlock rwlock = nullptr;
    std::atomic<bool> held{false};
    std::atomic<bool> release{false};
};

void* APS5_VABI ContentionWorker(void* arg) {
    auto* state = static_cast<ContentionState*>(arg);
    Require(pthread_rwlock_wrlock_nid_postfix(&state->rwlock) == 0);
    state->held.store(true, std::memory_order_release);
    while (!state->release.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    Require(pthread_rwlock_unlock_nid_postfix(&state->rwlock) == 0);
    return nullptr;
}

void TestContendedTimedWaitBounds() {
    ContentionState state;
    Require(pthread_rwlock_init_nid_postfix(&state.rwlock, nullptr) == 0);

    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, ContentionWorker, &state, "contention_w") == 0);

    while (!state.held.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    Require(pthread_rwlock_tryrdlock_nid_postfix(&state.rwlock) == PosixEbusy);
    Require(pthread_rwlock_trywrlock_nid_postfix(&state.rwlock) == PosixEbusy);
    Require(scePthreadRwlockTryrdlock(&state.rwlock) == SceKernelErrorEbusy);
    Require(scePthreadRwlockTrywrlock(&state.rwlock) == SceKernelErrorEbusy);

    Require(pthread_rwlock_unlock_nid_postfix(&state.rwlock) == PosixEperm);
    Require(scePthreadRwlockUnlock(&state.rwlock) == SceKernelErrorEperm);

    const KernelTimespec invalidTimespecs[] = {
        {0, -1},
        {5, -100},
        {0, 1000000000},
        {1, 1000000001},
        {2, 2000000000}
    };

    for (const auto& ts : invalidTimespecs) {
        Require(pthread_rwlock_timedrdlock_nid_postfix(&state.rwlock, &ts) == PosixEinval);
        Require(pthread_rwlock_timedwrlock_nid_postfix(&state.rwlock, &ts) == PosixEinval);
    }

    const KernelTimespec pastTimespecs[] = {
        {0, 0},
        {-1, 0},
        {-10, 50}
    };
    for (const auto& ts : pastTimespecs) {
        Require(pthread_rwlock_timedrdlock_nid_postfix(&state.rwlock, &ts) == PosixEtimedout);
        Require(pthread_rwlock_timedwrlock_nid_postfix(&state.rwlock, &ts) == PosixEtimedout);
    }

    Require(pthread_rwlock_reltimedrdlock_np_nid_postfix(&state.rwlock, 0) == PosixEtimedout);
    Require(pthread_rwlock_reltimedwrlock_np_nid_postfix(&state.rwlock, 0) == PosixEtimedout);
    Require(scePthreadRwlockTimedrdlock(&state.rwlock, 0) == SceKernelErrorEtimedout);
    Require(scePthreadRwlockTimedwrlock(&state.rwlock, 0) == SceKernelErrorEtimedout);

    state.release.store(true, std::memory_order_release);
    void* retVal = nullptr;
    Require(scePthreadJoin(thread, &retVal) == 0);

    const KernelTimespec validPast{0, 0};
    Require(pthread_rwlock_timedrdlock_nid_postfix(&state.rwlock, &validPast) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&state.rwlock) == 0);

    Require(pthread_rwlock_timedwrlock_nid_postfix(&state.rwlock, &validPast) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&state.rwlock) == 0);

    for (const auto& ts : invalidTimespecs) {
        Require(pthread_rwlock_timedrdlock_nid_postfix(&state.rwlock, &ts) == 0);
        Require(pthread_rwlock_unlock_nid_postfix(&state.rwlock) == 0);
        Require(pthread_rwlock_timedwrlock_nid_postfix(&state.rwlock, &ts) == 0);
        Require(pthread_rwlock_unlock_nid_postfix(&state.rwlock) == 0);
    }

    Require(pthread_rwlock_destroy_nid_postfix(&state.rwlock) == 0);
}

void TestRwlockSetNameBounds() {
    PthreadRwlock lock = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&lock, nullptr) == 0);

    Require(pthread_rwlock_setname_np_nid_postfix(&lock, nullptr) == PosixEinval);
    Require(pthread_rwlock_setname_np_nid_postfix(&lock, "audit_rwlock") == 0);
    Require(pthread_rwlock_setname_np_nid_postfix(&lock, "audit_rwlock") == PosixEagain);
    Require(pthread_rwlock_setname_np_nid_postfix(&lock, "other_name") == PosixEagain);

    Require(pthread_rwlock_destroy_nid_postfix(&lock) == 0);
}

} // namespace

int main() {
    TestRwlockattrTypesAndValidation();
    TestReadWriteLockingPrecedence();
    TestContendedTimedWaitBounds();
    TestRwlockSetNameBounds();
    return 0;
}
