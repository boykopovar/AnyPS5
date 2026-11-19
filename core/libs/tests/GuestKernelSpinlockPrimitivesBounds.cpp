#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <thread>

namespace {

constexpr int PosixEperm = 1;
constexpr int PosixEfault = 14;
constexpr int PosixEbusy = 16;
constexpr int PosixEinval = 22;

constexpr int SceKernelErrorEfault = 0x8002000E;
constexpr int SceKernelErrorEinval = 0x80020016;

constexpr int PthreadProcessPrivate = 0;
constexpr int PthreadProcessShared = 1;
constexpr int MutexTypeAdaptive = 4;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestSpinlockLifecycleAndValidation() {
    Require(pthread_spin_init_nid_postfix(nullptr, PthreadProcessPrivate) == PosixEinval);

    PthreadSpinlockPrivate* lock = nullptr;
    Require(pthread_spin_init_nid_postfix(&lock, PthreadProcessShared) == PosixEinval);
    Require(pthread_spin_init_nid_postfix(&lock, -1) == PosixEinval);
    Require(pthread_spin_init_nid_postfix(&lock, 2) == PosixEinval);
    Require(pthread_spin_init_nid_postfix(&lock, 100) == PosixEinval);
    Require(lock == nullptr);

    Require(pthread_spin_lock_nid_postfix(nullptr) == PosixEinval);
    Require(pthread_spin_lock_nid_postfix(&lock) == PosixEinval);

    Require(pthread_spin_trylock_nid_postfix(nullptr) == PosixEinval);
    Require(pthread_spin_trylock_nid_postfix(&lock) == PosixEinval);

    Require(pthread_spin_unlock_nid_postfix(nullptr) == PosixEinval);
    Require(pthread_spin_unlock_nid_postfix(&lock) == PosixEinval);

    Require(pthread_spin_destroy_nid_postfix(nullptr) == PosixEinval);
    Require(pthread_spin_destroy_nid_postfix(&lock) == PosixEinval);

    Require(pthread_spin_init_nid_postfix(&lock, PthreadProcessPrivate) == 0);
    Require(lock != nullptr);

    Require(pthread_spin_unlock_nid_postfix(&lock) == PosixEperm);

    Require(pthread_spin_lock_nid_postfix(&lock) == 0);
    Require(pthread_spin_trylock_nid_postfix(&lock) == PosixEbusy);

    Require(pthread_spin_unlock_nid_postfix(&lock) == 0);
    Require(pthread_spin_unlock_nid_postfix(&lock) == PosixEperm);

    Require(pthread_spin_trylock_nid_postfix(&lock) == 0);
    Require(pthread_spin_unlock_nid_postfix(&lock) == 0);

    Require(pthread_spin_destroy_nid_postfix(&lock) == 0);
    Require(lock == nullptr);

    Require(pthread_spin_destroy_nid_postfix(&lock) == PosixEinval);
    Require(pthread_spin_lock_nid_postfix(&lock) == PosixEinval);
    Require(pthread_spin_trylock_nid_postfix(&lock) == PosixEinval);
    Require(pthread_spin_unlock_nid_postfix(&lock) == PosixEinval);
}

struct ContenderState {
    PthreadSpinlockPrivate* lock = nullptr;
    std::atomic<bool> held{false};
    std::atomic<bool> release{false};
};

void* APS5_VABI ContenderWorker(void* arg) {
    auto* state = static_cast<ContenderState*>(arg);
    Require(pthread_spin_lock_nid_postfix(&state->lock) == 0);
    state->held.store(true, std::memory_order_release);
    while (!state->release.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    Require(pthread_spin_unlock_nid_postfix(&state->lock) == 0);
    return nullptr;
}

void TestSpinlockContention() {
    ContenderState state;
    Require(pthread_spin_init_nid_postfix(&state.lock, PthreadProcessPrivate) == 0);

    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, ContenderWorker, &state, "contender") == 0);

    while (!state.held.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    Require(pthread_spin_trylock_nid_postfix(&state.lock) == PosixEbusy);
    Require(pthread_spin_unlock_nid_postfix(&state.lock) == PosixEperm);

    state.release.store(true, std::memory_order_release);
    void* retVal = nullptr;
    Require(scePthreadJoin(thread, &retVal) == 0);

    Require(pthread_spin_trylock_nid_postfix(&state.lock) == 0);
    Require(pthread_spin_unlock_nid_postfix(&state.lock) == 0);
    Require(pthread_spin_destroy_nid_postfix(&state.lock) == 0);
    Require(state.lock == nullptr);
}

void TestMutexSpinLoopsTwinApis() {
    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);

    int spins = -1;
    Require(pthread_mutex_getspinloops_np_nid_postfix(&mutex, &spins) == 0 && spins == 0);
    Require(scePthreadMutexGetspinloops(&mutex, &spins) == 0 && spins == 0);

    int yields = -1;
    Require(pthread_mutex_getyieldloops_np_nid_postfix(&mutex, &yields) == 0 && yields == 0);
    Require(scePthreadMutexGetyieldloops(&mutex, &yields) == 0 && yields == 0);

    Require(pthread_mutex_setspinloops_np_nid_postfix(&mutex, 1000) == 0);
    Require(pthread_mutex_getspinloops_np_nid_postfix(&mutex, &spins) == 0 && spins == 1000);
    Require(scePthreadMutexGetspinloops(&mutex, &spins) == 0 && spins == 1000);

    Require(scePthreadMutexSetspinloops(&mutex, 2500) == 0);
    Require(pthread_mutex_getspinloops_np_nid_postfix(&mutex, &spins) == 0 && spins == 2500);
    Require(scePthreadMutexGetspinloops(&mutex, &spins) == 0 && spins == 2500);

    Require(pthread_mutex_setyieldloops_np_nid_postfix(&mutex, 50) == 0);
    Require(pthread_mutex_getyieldloops_np_nid_postfix(&mutex, &yields) == 0 && yields == 50);
    Require(scePthreadMutexGetyieldloops(&mutex, &yields) == 0 && yields == 50);

    Require(scePthreadMutexSetyieldloops(&mutex, 100) == 0);
    Require(pthread_mutex_getyieldloops_np_nid_postfix(&mutex, &yields) == 0 && yields == 100);
    Require(scePthreadMutexGetyieldloops(&mutex, &yields) == 0 && yields == 100);

    Require(pthread_mutex_setspinloops_np_nid_postfix(&mutex, -5) == 0);
    Require(pthread_mutex_getspinloops_np_nid_postfix(&mutex, &spins) == 0 && spins == -5);

    Require(pthread_mutex_setyieldloops_np_nid_postfix(&mutex, -20) == 0);
    Require(pthread_mutex_getyieldloops_np_nid_postfix(&mutex, &yields) == 0 && yields == -20);

    Require(pthread_mutex_getspinloops_np_nid_postfix(&mutex, nullptr) == PosixEfault);
    Require(scePthreadMutexGetspinloops(&mutex, nullptr) == SceKernelErrorEfault);
    Require(pthread_mutex_getyieldloops_np_nid_postfix(&mutex, nullptr) == PosixEfault);
    Require(scePthreadMutexGetyieldloops(&mutex, nullptr) == SceKernelErrorEfault);

    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);

    Require(pthread_mutex_getspinloops_np_nid_postfix(&mutex, &spins) == PosixEinval);
    Require(scePthreadMutexGetspinloops(&mutex, &spins) == SceKernelErrorEinval);
    Require(pthread_mutex_setspinloops_np_nid_postfix(&mutex, 100) == PosixEinval);
    Require(scePthreadMutexSetspinloops(&mutex, 100) == SceKernelErrorEinval);

    Require(pthread_mutex_getyieldloops_np_nid_postfix(&mutex, &yields) == PosixEinval);
    Require(scePthreadMutexGetyieldloops(&mutex, &yields) == SceKernelErrorEinval);
    Require(pthread_mutex_setyieldloops_np_nid_postfix(&mutex, 100) == PosixEinval);
    Require(scePthreadMutexSetyieldloops(&mutex, 100) == SceKernelErrorEinval);

    Require(pthread_mutex_getspinloops_np_nid_postfix(nullptr, &spins) == PosixEinval);
    Require(scePthreadMutexGetspinloops(nullptr, &spins) == SceKernelErrorEinval);
    Require(pthread_mutex_setspinloops_np_nid_postfix(nullptr, 100) == PosixEinval);
    Require(scePthreadMutexSetspinloops(nullptr, 100) == SceKernelErrorEinval);

    Require(pthread_mutex_getyieldloops_np_nid_postfix(nullptr, &yields) == PosixEinval);
    Require(scePthreadMutexGetyieldloops(nullptr, &yields) == SceKernelErrorEinval);
    Require(pthread_mutex_setyieldloops_np_nid_postfix(nullptr, 100) == PosixEinval);
    Require(scePthreadMutexSetyieldloops(nullptr, 100) == SceKernelErrorEinval);
}

void TestAdaptiveMutexSpinLoops() {
    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_settype_nid_postfix(&attr, MutexTypeAdaptive) == 0);

    PthreadMutex adaptive = nullptr;
    Require(pthread_mutex_init_nid_postfix(&adaptive, &attr) == 0);

    int spins = -1;
    Require(pthread_mutex_getspinloops_np_nid_postfix(&adaptive, &spins) == 0 && spins == 2000);
    Require(scePthreadMutexGetspinloops(&adaptive, &spins) == 0 && spins == 2000);

    Require(pthread_mutex_destroy_nid_postfix(&adaptive) == 0);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);

    auto staticAdaptive = reinterpret_cast<PthreadMutex>(std::uintptr_t{1});
    Require(scePthreadMutexGetspinloops(&staticAdaptive, &spins) == 0 && spins == 2000);
    Require(scePthreadMutexGetyieldloops(&staticAdaptive, &spins) == 0 && spins == 0);
    Require(scePthreadMutexDestroy(&staticAdaptive) == 0);
}

} // namespace

int main() {
    TestSpinlockLifecycleAndValidation();
    TestSpinlockContention();
    TestMutexSpinLoopsTwinApis();
    TestAdaptiveMutexSpinLoops();
    return 0;
}
