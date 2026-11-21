#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <exception>

namespace {

constexpr int PosixEperm = 1;
constexpr int PosixEbusy = 16;
constexpr int PosixEinval = 22;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

struct SpinContext {
    PthreadSpinlockPrivate* lock = nullptr;
    std::atomic<bool> contenderTrying{false};
    std::atomic<bool> contenderAcquired{false};
    std::atomic<int> contenderTrylockResult{-1};
    std::atomic<int> contenderUnlockResult{-1};
};

void* APS5_VABI ContenderThread(void* arg) {
    auto* ctx = static_cast<SpinContext*>(arg);
    ctx->contenderTrylockResult.store(pthread_spin_trylock_nid_postfix(&ctx->lock));
    ctx->contenderUnlockResult.store(pthread_spin_unlock_nid_postfix(&ctx->lock));
    ctx->contenderTrying.store(true);
    Require(pthread_spin_lock_nid_postfix(&ctx->lock) == 0);
    ctx->contenderAcquired.store(true);
    Require(pthread_spin_unlock_nid_postfix(&ctx->lock) == 0);
    return nullptr;
}

void TestParameterBoundsAndPshared() {
    Require(pthread_spin_init_nid_postfix(nullptr, 0) == PosixEinval);

    PthreadSpinlockPrivate* lock = nullptr;
    const int invalidPshared[] = {-100, -1, 1, 2, 42, 9999};
    for (const int pshared : invalidPshared) {
        Require(pthread_spin_init_nid_postfix(&lock, pshared) == PosixEinval);
        Require(lock == nullptr);
    }

    Require(pthread_spin_init_nid_postfix(&lock, 0) == 0);
    Require(lock != nullptr);
    Require(pthread_spin_destroy_nid_postfix(&lock) == 0);
    Require(lock == nullptr);
}

void TestUninitializedAndDestroyedHandleRejection() {
    Require(pthread_spin_destroy_nid_postfix(nullptr) == PosixEinval);
    Require(pthread_spin_trylock_nid_postfix(nullptr) == PosixEinval);
    Require(pthread_spin_lock_nid_postfix(nullptr) == PosixEinval);
    Require(pthread_spin_unlock_nid_postfix(nullptr) == PosixEinval);

    PthreadSpinlockPrivate* uninit = nullptr;
    Require(pthread_spin_destroy_nid_postfix(&uninit) == PosixEinval);
    Require(pthread_spin_trylock_nid_postfix(&uninit) == PosixEinval);
    Require(pthread_spin_lock_nid_postfix(&uninit) == PosixEinval);
    Require(pthread_spin_unlock_nid_postfix(&uninit) == PosixEinval);

    PthreadSpinlockPrivate* destroyed = nullptr;
    Require(pthread_spin_init_nid_postfix(&destroyed, 0) == 0);
    Require(destroyed != nullptr);
    Require(pthread_spin_destroy_nid_postfix(&destroyed) == 0);
    Require(destroyed == nullptr);

    Require(pthread_spin_destroy_nid_postfix(&destroyed) == PosixEinval);
    Require(pthread_spin_trylock_nid_postfix(&destroyed) == PosixEinval);
    Require(pthread_spin_lock_nid_postfix(&destroyed) == PosixEinval);
    Require(pthread_spin_unlock_nid_postfix(&destroyed) == PosixEinval);
}

void TestSingleThreadLockUnlockAndRecursiveDeadlock() {
    PthreadSpinlockPrivate* lock = nullptr;
    Require(pthread_spin_init_nid_postfix(&lock, 0) == 0);
    Require(lock != nullptr);

    Require(pthread_spin_unlock_nid_postfix(&lock) == PosixEperm);

    Require(pthread_spin_trylock_nid_postfix(&lock) == 0);
    Require(pthread_spin_trylock_nid_postfix(&lock) == PosixEbusy);
    Require(Throws([&] { pthread_spin_lock_nid_postfix(&lock); }));

    Require(pthread_spin_unlock_nid_postfix(&lock) == 0);
    Require(pthread_spin_unlock_nid_postfix(&lock) == PosixEperm);

    Require(pthread_spin_lock_nid_postfix(&lock) == 0);
    Require(pthread_spin_trylock_nid_postfix(&lock) == PosixEbusy);
    Require(Throws([&] { pthread_spin_lock_nid_postfix(&lock); }));

    Require(pthread_spin_unlock_nid_postfix(&lock) == 0);
    Require(pthread_spin_unlock_nid_postfix(&lock) == PosixEperm);

    Require(pthread_spin_destroy_nid_postfix(&lock) == 0);
    Require(lock == nullptr);
}

void TestContentionAndConcurrentThreads() {
    SpinContext ctx;
    Require(pthread_spin_init_nid_postfix(&ctx.lock, 0) == 0);
    Require(ctx.lock != nullptr);

    Require(pthread_spin_lock_nid_postfix(&ctx.lock) == 0);

    Pthread contender = nullptr;
    Require(scePthreadCreate(&contender, nullptr, ContenderThread, &ctx, nullptr) == 0);

    while (!ctx.contenderTrying.load()) {
        scePthreadYield();
    }

    Require(ctx.contenderTrylockResult.load() == PosixEbusy);
    Require(ctx.contenderUnlockResult.load() == PosixEperm);
    Require(!ctx.contenderAcquired.load());

    Require(pthread_spin_unlock_nid_postfix(&ctx.lock) == 0);

    while (!ctx.contenderAcquired.load()) {
        scePthreadYield();
    }

    Require(scePthreadJoin(contender, nullptr) == 0);

    Require(pthread_spin_trylock_nid_postfix(&ctx.lock) == 0);
    Require(pthread_spin_unlock_nid_postfix(&ctx.lock) == 0);

    Require(pthread_spin_destroy_nid_postfix(&ctx.lock) == 0);
    Require(ctx.lock == nullptr);
}

} // namespace

int main() {
    TestParameterBoundsAndPshared();
    TestUninitializedAndDestroyedHandleRejection();
    TestSingleThreadLockUnlockAndRecursiveDeadlock();
    TestContentionAndConcurrentThreads();
    return 0;
}
