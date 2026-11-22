#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

constexpr int PosixEperm = 1;
constexpr int PosixEdeadlk = 11;
constexpr int PosixEbusy = 16;
constexpr int PosixEinval = 22;
constexpr int PosixEtimedout = 60;

constexpr int MutexTypeErrorCheck = 1;
constexpr int MutexTypeRecursive = 2;
constexpr int MutexTypeNormal = 3;
constexpr int MutexTypeAdaptive = 4;

void Require(bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Require failed at line %u\n", static_cast<unsigned>(location.line()));
        std::fflush(stderr);
        std::abort();
    }
}

struct HolderContext {
    PthreadMutex mutex = nullptr;
    std::atomic<bool> locked{false};
    std::atomic<bool> release{false};
    std::atomic<int> workerTrylock{-1};
    std::atomic<int> workerTimedlock{-1};
    std::atomic<int> workerUnlock{-1};
    std::atomic<int> workerDestroy{-1};
};

void* APS5_VABI HolderThread(void* arg) {
    auto* ctx = static_cast<HolderContext*>(arg);
    Require(pthread_mutex_lock_nid_postfix(&ctx->mutex) == 0);
    ctx->locked.store(true);
    while (!ctx->release.load()) {
        scePthreadYield();
    }
    Require(pthread_mutex_unlock_nid_postfix(&ctx->mutex) == 0);
    return nullptr;
}

void* APS5_VABI ContenderThread(void* arg) {
    auto* ctx = static_cast<HolderContext*>(arg);
    const KernelTimespec past{0, 0};
    ctx->workerTrylock.store(pthread_mutex_trylock_nid_postfix(&ctx->mutex));
    ctx->workerTimedlock.store(pthread_mutex_timedlock_nid_postfix(&ctx->mutex, &past));
    ctx->workerUnlock.store(pthread_mutex_unlock_nid_postfix(&ctx->mutex));
    ctx->workerDestroy.store(pthread_mutex_destroy_nid_postfix(&ctx->mutex));
    return nullptr;
}

void TestParameterBoundsAndDestroy() {
    PthreadMutex uninit = nullptr;
    Require(pthread_mutex_destroy_nid_postfix(&uninit) == 0);

    PthreadMutex lazy = nullptr;
    Require(pthread_mutex_trylock_nid_postfix(&lazy) == 0);
    Require(lazy != nullptr);
    Require(pthread_mutex_unlock_nid_postfix(&lazy) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&lazy) == 0);

    PthreadMutex destroyed = nullptr;
    Require(pthread_mutex_init_nid_postfix(&destroyed, nullptr) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&destroyed) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&destroyed) == PosixEinval);
    Require(pthread_mutex_trylock_nid_postfix(&destroyed) == PosixEinval);
    Require(pthread_mutex_lock_nid_postfix(&destroyed) == PosixEinval);
    Require(pthread_mutex_unlock_nid_postfix(&destroyed) == PosixEinval);

    const KernelTimespec past{0, 0};
    Require(pthread_mutex_timedlock_nid_postfix(&destroyed, &past) == PosixEinval);

    PthreadMutex held = nullptr;
    Require(pthread_mutex_init_nid_postfix(&held, nullptr) == 0);
    Require(pthread_mutex_lock_nid_postfix(&held) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&held) == PosixEbusy);
    Require(pthread_mutex_unlock_nid_postfix(&held) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&held) == 0);
}

void TestTimedlockParameterBounds() {
    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);

    const KernelTimespec past{0, 0};
    Require(pthread_mutex_timedlock_nid_postfix(&mutex, &past) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);

    Require(pthread_mutex_timedlock_nid_postfix(&mutex, nullptr) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);

    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);

    const KernelTimespec invalidNsecs[] = {
        {100, -1},
        {0, -100},
        {100, 1000000000},
        {100, 1000000001},
        {100, 2000000000}
    };

    for (const auto& inv : invalidNsecs) {
        Require(pthread_mutex_timedlock_nid_postfix(&mutex, &inv) == PosixEinval);
    }

    Require(pthread_mutex_timedlock_nid_postfix(&mutex, &past) == PosixEtimedout);

    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
}

void TestLockContentionBetweenThreads() {
    HolderContext ctx;
    Require(pthread_mutex_init_nid_postfix(&ctx.mutex, nullptr) == 0);

    Pthread holder = nullptr;
    Require(scePthreadCreate(&holder, nullptr, HolderThread, &ctx, nullptr) == 0);
    while (!ctx.locked.load()) {
        scePthreadYield();
    }

    const KernelTimespec past{0, 0};
    const KernelTimespec invalidNsec{100, 1000000000};

    Require(pthread_mutex_trylock_nid_postfix(&ctx.mutex) == PosixEbusy);
    Require(pthread_mutex_timedlock_nid_postfix(&ctx.mutex, &past) == PosixEtimedout);
    Require(pthread_mutex_timedlock_nid_postfix(&ctx.mutex, &invalidNsec) == PosixEinval);
    Require(pthread_mutex_unlock_nid_postfix(&ctx.mutex) == PosixEperm);
    Require(pthread_mutex_destroy_nid_postfix(&ctx.mutex) == PosixEbusy);

    Pthread contender = nullptr;
    Require(scePthreadCreate(&contender, nullptr, ContenderThread, &ctx, nullptr) == 0);
    Require(scePthreadJoin(contender, nullptr) == 0);

    Require(ctx.workerTrylock.load() == PosixEbusy);
    Require(ctx.workerTimedlock.load() == PosixEtimedout);
    Require(ctx.workerUnlock.load() == PosixEperm);
    Require(ctx.workerDestroy.load() == PosixEbusy);

    ctx.release.store(true);
    Require(scePthreadJoin(holder, nullptr) == 0);

    Require(pthread_mutex_trylock_nid_postfix(&ctx.mutex) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&ctx.mutex) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&ctx.mutex) == 0);
}

void TestRecursiveErrorPrecedence() {
    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);

    {
        Require(pthread_mutexattr_settype_nid_postfix(&attr, MutexTypeErrorCheck) == 0);
        PthreadMutex mutex = nullptr;
        Require(pthread_mutex_init_nid_postfix(&mutex, &attr) == 0);

        Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
        Require(pthread_mutex_trylock_nid_postfix(&mutex) == PosixEbusy);
        Require(pthread_mutex_lock_nid_postfix(&mutex) == PosixEdeadlk);

        Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
        Require(pthread_mutex_unlock_nid_postfix(&mutex) == PosixEperm);
        Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    }

    {
        Require(pthread_mutexattr_settype_nid_postfix(&attr, MutexTypeAdaptive) == 0);
        PthreadMutex mutex = nullptr;
        Require(pthread_mutex_init_nid_postfix(&mutex, &attr) == 0);

        Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
        Require(pthread_mutex_trylock_nid_postfix(&mutex) == PosixEinval);
        Require(pthread_mutex_lock_nid_postfix(&mutex) == PosixEdeadlk);

        Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
        Require(pthread_mutex_unlock_nid_postfix(&mutex) == PosixEperm);
        Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    }

    {
        Require(pthread_mutexattr_settype_nid_postfix(&attr, MutexTypeNormal) == 0);
        PthreadMutex mutex = nullptr;
        Require(pthread_mutex_init_nid_postfix(&mutex, &attr) == 0);

        Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
        Require(pthread_mutex_trylock_nid_postfix(&mutex) == PosixEbusy);

        Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
        Require(pthread_mutex_unlock_nid_postfix(&mutex) == PosixEperm);
        Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    }

    {
        Require(pthread_mutexattr_settype_nid_postfix(&attr, MutexTypeRecursive) == 0);
        PthreadMutex mutex = nullptr;
        Require(pthread_mutex_init_nid_postfix(&mutex, &attr) == 0);

        Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
        Require(pthread_mutex_trylock_nid_postfix(&mutex) == 0);
        Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);

        Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
        Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
        Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
        Require(pthread_mutex_unlock_nid_postfix(&mutex) == PosixEperm);
        Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    }

    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
}

} // namespace

int main() {
    TestParameterBoundsAndDestroy();
    TestTimedlockParameterBounds();
    TestLockContentionBetweenThreads();
    TestRecursiveErrorPrecedence();
    return 0;
}
