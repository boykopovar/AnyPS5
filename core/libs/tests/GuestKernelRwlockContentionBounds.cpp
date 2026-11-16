#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <exception>

namespace {

constexpr int PosixEperm = 1;
constexpr int PosixEdeadlk = 11;
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

struct ThreadContext {
    PthreadRwlock rwlock = nullptr;
    std::atomic<bool> writeLocked{false};
    std::atomic<bool> readLocked{false};
    std::atomic<bool> release{false};
    std::atomic<int> workerTryrdlock{-1};
    std::atomic<int> workerTrywrlock{-1};
    std::atomic<int> workerUnlock{-1};
};

void* APS5_VABI WriteHolderThread(void* arg) {
    auto* ctx = static_cast<ThreadContext*>(arg);
    Require(pthread_rwlock_wrlock_nid_postfix(&ctx->rwlock) == 0);
    ctx->writeLocked.store(true);
    while (!ctx->release.load()) {
        scePthreadYield();
    }
    Require(pthread_rwlock_unlock_nid_postfix(&ctx->rwlock) == 0);
    return nullptr;
}

void* APS5_VABI ReadHolderThread(void* arg) {
    auto* ctx = static_cast<ThreadContext*>(arg);
    Require(pthread_rwlock_rdlock_nid_postfix(&ctx->rwlock) == 0);
    ctx->readLocked.store(true);
    while (!ctx->release.load()) {
        scePthreadYield();
    }
    Require(pthread_rwlock_unlock_nid_postfix(&ctx->rwlock) == 0);
    return nullptr;
}

void* APS5_VABI ContenderThread(void* arg) {
    auto* ctx = static_cast<ThreadContext*>(arg);
    ctx->workerTryrdlock.store(pthread_rwlock_tryrdlock_nid_postfix(&ctx->rwlock));
    ctx->workerTrywrlock.store(pthread_rwlock_trywrlock_nid_postfix(&ctx->rwlock));
    ctx->workerUnlock.store(pthread_rwlock_unlock_nid_postfix(&ctx->rwlock));
    return nullptr;
}

void TestParameterBoundsAndDestroy() {
    Require(Throws([] { pthread_rwlock_destroy_nid_postfix(nullptr); }));
    Require(Throws([] { pthread_rwlock_tryrdlock_nid_postfix(nullptr); }));
    Require(Throws([] { pthread_rwlock_trywrlock_nid_postfix(nullptr); }));
    Require(Throws([] { pthread_rwlock_wrlock_nid_postfix(nullptr); }));
    Require(Throws([] { pthread_rwlock_rdlock_nid_postfix(nullptr); }));
    Require(Throws([] { pthread_rwlock_unlock_nid_postfix(nullptr); }));

    PthreadRwlock uninit = nullptr;
    Require(pthread_rwlock_destroy_nid_postfix(&uninit) == 0);

    PthreadRwlock lazyRd = nullptr;
    Require(pthread_rwlock_tryrdlock_nid_postfix(&lazyRd) == 0);
    Require(lazyRd != nullptr);
    Require(pthread_rwlock_unlock_nid_postfix(&lazyRd) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&lazyRd) == 0);

    PthreadRwlock lazyWr = nullptr;
    Require(pthread_rwlock_trywrlock_nid_postfix(&lazyWr) == 0);
    Require(lazyWr != nullptr);
    Require(pthread_rwlock_unlock_nid_postfix(&lazyWr) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&lazyWr) == 0);

    PthreadRwlock destroyed = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&destroyed, nullptr) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&destroyed) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&destroyed) == PosixEinval);
    Require(pthread_rwlock_tryrdlock_nid_postfix(&destroyed) == PosixEinval);
    Require(pthread_rwlock_trywrlock_nid_postfix(&destroyed) == PosixEinval);
    Require(pthread_rwlock_rdlock_nid_postfix(&destroyed) == PosixEinval);
    Require(pthread_rwlock_wrlock_nid_postfix(&destroyed) == PosixEinval);
    Require(pthread_rwlock_unlock_nid_postfix(&destroyed) == PosixEinval);
}

void TestRecursiveWriteLockAndEdeadlk() {
    PthreadRwlock rwlock = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&rwlock, nullptr) == 0);

    Require(pthread_rwlock_wrlock_nid_postfix(&rwlock) == 0);

    Require(pthread_rwlock_wrlock_nid_postfix(&rwlock) == PosixEdeadlk);
    Require(pthread_rwlock_rdlock_nid_postfix(&rwlock) == PosixEdeadlk);

    Require(pthread_rwlock_trywrlock_nid_postfix(&rwlock) == PosixEbusy);
    Require(pthread_rwlock_tryrdlock_nid_postfix(&rwlock) == PosixEbusy);

    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == PosixEperm);

    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == 0);
}

void TestWriteLockContention() {
    ThreadContext ctx;
    Require(pthread_rwlock_init_nid_postfix(&ctx.rwlock, nullptr) == 0);

    Pthread writer = nullptr;
    Require(scePthreadCreate(&writer, nullptr, WriteHolderThread, &ctx, nullptr) == 0);
    while (!ctx.writeLocked.load()) {
        scePthreadYield();
    }

    Require(pthread_rwlock_tryrdlock_nid_postfix(&ctx.rwlock) == PosixEbusy);
    Require(pthread_rwlock_trywrlock_nid_postfix(&ctx.rwlock) == PosixEbusy);
    Require(pthread_rwlock_unlock_nid_postfix(&ctx.rwlock) == PosixEperm);

    Pthread contender = nullptr;
    Require(scePthreadCreate(&contender, nullptr, ContenderThread, &ctx, nullptr) == 0);
    Require(scePthreadJoin(contender, nullptr) == 0);

    Require(ctx.workerTryrdlock.load() == PosixEbusy);
    Require(ctx.workerTrywrlock.load() == PosixEbusy);
    Require(ctx.workerUnlock.load() == PosixEperm);

    ctx.release.store(true);
    Require(scePthreadJoin(writer, nullptr) == 0);

    Require(pthread_rwlock_trywrlock_nid_postfix(&ctx.rwlock) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&ctx.rwlock) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&ctx.rwlock) == 0);
}

void TestReadLockContentionAndSharing() {
    ThreadContext ctx;
    Require(pthread_rwlock_init_nid_postfix(&ctx.rwlock, nullptr) == 0);

    Pthread reader = nullptr;
    Require(scePthreadCreate(&reader, nullptr, ReadHolderThread, &ctx, nullptr) == 0);
    while (!ctx.readLocked.load()) {
        scePthreadYield();
    }

    Require(pthread_rwlock_tryrdlock_nid_postfix(&ctx.rwlock) == 0);
    Require(pthread_rwlock_trywrlock_nid_postfix(&ctx.rwlock) == PosixEbusy);
    Require(pthread_rwlock_unlock_nid_postfix(&ctx.rwlock) == 0);

    ctx.release.store(true);
    Require(scePthreadJoin(reader, nullptr) == 0);

    Require(pthread_rwlock_trywrlock_nid_postfix(&ctx.rwlock) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&ctx.rwlock) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&ctx.rwlock) == 0);
}

void TestDestroyWhileHeld() {
    PthreadRwlock heldRead = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&heldRead, nullptr) == 0);
    Require(pthread_rwlock_rdlock_nid_postfix(&heldRead) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&heldRead) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&heldRead) == PosixEinval);
    Require(pthread_rwlock_tryrdlock_nid_postfix(&heldRead) == PosixEinval);
    Require(pthread_rwlock_trywrlock_nid_postfix(&heldRead) == PosixEinval);
    Require(pthread_rwlock_unlock_nid_postfix(&heldRead) == PosixEinval);

    PthreadRwlock heldWrite = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&heldWrite, nullptr) == 0);
    Require(pthread_rwlock_wrlock_nid_postfix(&heldWrite) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&heldWrite) == 0);
    Require(pthread_rwlock_destroy_nid_postfix(&heldWrite) == PosixEinval);
    Require(pthread_rwlock_tryrdlock_nid_postfix(&heldWrite) == PosixEinval);
    Require(pthread_rwlock_trywrlock_nid_postfix(&heldWrite) == PosixEinval);
    Require(pthread_rwlock_unlock_nid_postfix(&heldWrite) == PosixEinval);
}

} // namespace

int main() {
    TestParameterBoundsAndDestroy();
    TestRecursiveWriteLockAndEdeadlk();
    TestWriteLockContention();
    TestReadLockContentionAndSharing();
    TestDestroyWhileHeld();
    return 0;
}
