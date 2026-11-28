#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Rwlock.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <thread>

namespace {

constexpr int PosixEperm = 1;
constexpr int PosixEdeadlk = 11;
constexpr int PosixEfault = 14;
constexpr int PosixEbusy = 16;
constexpr int PosixEinval = 22;
constexpr int PosixEagain = 35;
constexpr int PosixEtimedout = 60;

constexpr int SceKernelErrorEperm = static_cast<int>(0x80020001u);
constexpr int SceKernelErrorEdeadlk = static_cast<int>(0x8002000Bu);
constexpr int SceKernelErrorEfault = static_cast<int>(0x8002000Eu);
constexpr int SceKernelErrorEbusy = static_cast<int>(0x80020010u);
constexpr int SceKernelErrorEinval = static_cast<int>(0x80020016u);
constexpr int SceKernelErrorEtimedout = static_cast<int>(0x8002003Cu);

constexpr int ProcessPrivate = 0;
constexpr int RwlockTypeDefault = 0;
constexpr int RwlockTypeShared = 1;
constexpr int RwlockTypeExclusive = 2;

const auto DestroyedAttrMarker = reinterpret_cast<PthreadRwlockattr>(std::uintptr_t{1});
const auto DestroyedRwlockMarker = reinterpret_cast<PthreadRwlock>(std::uintptr_t{1});

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename TFunc>
bool ThrowsRuntimeError(TFunc func) {
    try {
        func();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

struct ThreadSyncContext {
    PthreadRwlock rwlock = nullptr;
    std::atomic<bool> held{false};
    std::atomic<bool> release{false};
};

void* APS5_VABI WriterWorker(void* arg) {
    auto* ctx = static_cast<ThreadSyncContext*>(arg);
    Require(pthread_rwlock_wrlock_nid_postfix(&ctx->rwlock) == 0);
    ctx->held.store(true, std::memory_order_release);
    while (!ctx->release.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    Require(pthread_rwlock_unlock_nid_postfix(&ctx->rwlock) == 0);
    return nullptr;
}

struct MultiReaderContext {
    PthreadRwlock rwlock = nullptr;
    std::atomic<int> readersEntered{0};
    std::atomic<bool> release{false};
};

void* APS5_VABI ReaderWorker(void* arg) {
    auto* ctx = static_cast<MultiReaderContext*>(arg);
    Require(pthread_rwlock_rdlock_nid_postfix(&ctx->rwlock) == 0);
    ctx->readersEntered.fetch_add(1, std::memory_order_release);
    while (!ctx->release.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    Require(pthread_rwlock_unlock_nid_postfix(&ctx->rwlock) == 0);
    return nullptr;
}

void TestRwlockattrLifecycleAndDefaults() {
    PthreadRwlockattr attr = nullptr;
    Require(pthread_rwlockattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    int pshared = -1;
    Require(pthread_rwlockattr_getpshared_nid_postfix(&attr, &pshared) == 0);
    Require(pshared == ProcessPrivate);

    pshared = -1;
    Require(scePthreadRwlockattrGetpshared(&attr, &pshared) == 0);
    Require(pshared == ProcessPrivate);

    int type = -1;
    Require(pthread_rwlockattr_gettype_np_nid_postfix(&attr, &type) == 0);
    Require(type == RwlockTypeDefault);

    type = -1;
    Require(scePthreadRwlockattrGettype(&attr, &type) == 0);
    Require(type == RwlockTypeDefault);

    Require(pthread_rwlockattr_destroy_nid_postfix(&attr) == 0);
    Require(attr == DestroyedAttrMarker);

    PthreadRwlockattr sceAttr = nullptr;
    Require(scePthreadRwlockattrInit(&sceAttr) == 0);
    Require(sceAttr != nullptr);
    Require(scePthreadRwlockattrDestroy(&sceAttr) == 0);
    Require(sceAttr == nullptr);
}

void TestRwlockattrTypeValidationAndBounds() {
    PthreadRwlockattr attr = nullptr;
    Require(pthread_rwlockattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    Require(pthread_rwlockattr_settype_np_nid_postfix(&attr, RwlockTypeShared) == 0);
    int type = -1;
    Require(pthread_rwlockattr_gettype_np_nid_postfix(&attr, &type) == 0);
    Require(type == RwlockTypeShared);

    Require(pthread_rwlockattr_settype_np_nid_postfix(&attr, RwlockTypeExclusive) == 0);
    Require(pthread_rwlockattr_gettype_np_nid_postfix(&attr, &type) == 0);
    Require(type == RwlockTypeExclusive);

    Require(scePthreadRwlockattrSettype(&attr, RwlockTypeShared) == 0);
    Require(scePthreadRwlockattrGettype(&attr, &type) == 0);
    Require(type == RwlockTypeShared);

    const int invalidTypes[] = {-1000, -1, 0, 3, 4, 10, 100, 9999};
    for (const int badType : invalidTypes) {
        Require(pthread_rwlockattr_settype_np_nid_postfix(&attr, badType) == PosixEinval);
        Require(pthread_rwlockattr_gettype_np_nid_postfix(&attr, &type) == 0);
        Require(type == RwlockTypeShared);

        Require(scePthreadRwlockattrSettype(&attr, badType) == SceKernelErrorEinval);
        Require(scePthreadRwlockattrGettype(&attr, &type) == 0);
        Require(type == RwlockTypeShared);
    }

    Require(pthread_rwlockattr_destroy_nid_postfix(&attr) == 0);
}

void TestRwlockattrPsharedValidationAndBounds() {
    PthreadRwlockattr attr = nullptr;
    Require(pthread_rwlockattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    Require(pthread_rwlockattr_setpshared_nid_postfix(&attr, ProcessPrivate) == 0);
    Require(scePthreadRwlockattrSetpshared(&attr, ProcessPrivate) == 0);

    const int invalidPshared[] = {-100, -1, 1, 2, 42, 100, 9999};
    for (const int badPshared : invalidPshared) {
        Require(pthread_rwlockattr_setpshared_nid_postfix(&attr, badPshared) == PosixEinval);
        Require(scePthreadRwlockattrSetpshared(&attr, badPshared) == SceKernelErrorEinval);

        int pshared = -1;
        Require(pthread_rwlockattr_getpshared_nid_postfix(&attr, &pshared) == 0);
        Require(pshared == ProcessPrivate);
    }

    Require(pthread_rwlockattr_destroy_nid_postfix(&attr) == 0);
}

void TestRwlockattrNullAndDestroyedPrecedence() {
    Require(ThrowsRuntimeError([] { return pthread_rwlockattr_init_nid_postfix(nullptr); }));
    Require(ThrowsRuntimeError([] { return pthread_rwlockattr_destroy_nid_postfix(nullptr); }));
    Require(ThrowsRuntimeError([] { int pshared = 0; return pthread_rwlockattr_getpshared_nid_postfix(nullptr, &pshared); }));
    Require(ThrowsRuntimeError([] { int type = 0; return pthread_rwlockattr_gettype_np_nid_postfix(nullptr, &type); }));
    Require(ThrowsRuntimeError([] { return pthread_rwlockattr_setpshared_nid_postfix(nullptr, ProcessPrivate); }));
    Require(ThrowsRuntimeError([] { return scePthreadRwlockattrInit(nullptr); }));
    Require(ThrowsRuntimeError([] { return scePthreadRwlockattrDestroy(nullptr); }));

    Require(pthread_rwlockattr_settype_np_nid_postfix(nullptr, RwlockTypeShared) == PosixEinval);
    Require(scePthreadRwlockattrSettype(nullptr, RwlockTypeShared) == SceKernelErrorEinval);

    PthreadRwlockattr attr = nullptr;
    Require(pthread_rwlockattr_init_nid_postfix(&attr) == 0);
    Require(attr != nullptr);

    Require(pthread_rwlockattr_getpshared_nid_postfix(&attr, nullptr) == PosixEfault);
    Require(pthread_rwlockattr_gettype_np_nid_postfix(&attr, nullptr) == PosixEfault);
    Require(scePthreadRwlockattrGetpshared(&attr, nullptr) == SceKernelErrorEfault);
    Require(scePthreadRwlockattrGettype(&attr, nullptr) == SceKernelErrorEfault);

    Require(pthread_rwlockattr_destroy_nid_postfix(&attr) == 0);
    Require(attr == DestroyedAttrMarker);

    Require(ThrowsRuntimeError([&] { return pthread_rwlockattr_destroy_nid_postfix(&attr); }));
    Require(ThrowsRuntimeError([&] { int type = 0; return pthread_rwlockattr_gettype_np_nid_postfix(&attr, &type); }));
    Require(pthread_rwlockattr_settype_np_nid_postfix(&attr, RwlockTypeShared) == PosixEinval);
    Require(scePthreadRwlockattrSettype(&attr, RwlockTypeShared) == SceKernelErrorEinval);
}

void TestRwlockInitAndDestroyLifecycle() {
    PthreadRwlock rwlock = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&rwlock, nullptr) == 0);
    Require(rwlock != nullptr);

    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == 0);
    Require(rwlock == DestroyedRwlockMarker);

    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == PosixEinval);
    Require(scePthreadRwlockDestroy(&rwlock) == SceKernelErrorEinval);

    Require(pthread_rwlock_rdlock_nid_postfix(&rwlock) == PosixEinval);
    Require(pthread_rwlock_wrlock_nid_postfix(&rwlock) == PosixEinval);
    Require(pthread_rwlock_tryrdlock_nid_postfix(&rwlock) == PosixEinval);
    Require(pthread_rwlock_trywrlock_nid_postfix(&rwlock) == PosixEinval);
    Require(pthread_rwlock_reltimedrdlock_np_nid_postfix(&rwlock, 1000) == PosixEinval);
    Require(pthread_rwlock_reltimedwrlock_np_nid_postfix(&rwlock, 1000) == PosixEinval);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == PosixEinval);

    Require(scePthreadRwlockRdlock(&rwlock) == SceKernelErrorEinval);
    Require(scePthreadRwlockWrlock(&rwlock) == SceKernelErrorEinval);
    Require(scePthreadRwlockTryrdlock(&rwlock) == SceKernelErrorEinval);
    Require(scePthreadRwlockTrywrlock(&rwlock) == SceKernelErrorEinval);
    Require(scePthreadRwlockTimedrdlock(&rwlock, 1000) == SceKernelErrorEinval);
    Require(scePthreadRwlockTimedwrlock(&rwlock, 1000) == SceKernelErrorEinval);
    Require(scePthreadRwlockUnlock(&rwlock) == SceKernelErrorEinval);

    Require(ThrowsRuntimeError([] { return pthread_rwlock_init_nid_postfix(nullptr, nullptr); }));
    Require(ThrowsRuntimeError([] { return pthread_rwlock_destroy_nid_postfix(nullptr); }));
    Require(ThrowsRuntimeError([] { return pthread_rwlock_rdlock_nid_postfix(nullptr); }));
    Require(ThrowsRuntimeError([] { return pthread_rwlock_wrlock_nid_postfix(nullptr); }));
    Require(ThrowsRuntimeError([] { return pthread_rwlock_tryrdlock_nid_postfix(nullptr); }));
    Require(ThrowsRuntimeError([] { return pthread_rwlock_trywrlock_nid_postfix(nullptr); }));
    Require(ThrowsRuntimeError([] { return pthread_rwlock_unlock_nid_postfix(nullptr); }));
    Require(ThrowsRuntimeError([] { return scePthreadRwlockInit(nullptr, nullptr, nullptr); }));
    Require(ThrowsRuntimeError([] { return scePthreadRwlockDestroy(nullptr); }));
}

void TestRwlockSetNameBounds() {
    Require(ThrowsRuntimeError([] { return pthread_rwlock_setname_np_nid_postfix(nullptr, "WorkerRwlock"); }));

    PthreadRwlock uninit = nullptr;
    Require(ThrowsRuntimeError([&] { return pthread_rwlock_setname_np_nid_postfix(&uninit, "WorkerRwlock"); }));

    PthreadRwlock rwlock = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&rwlock, nullptr) == 0);
    Require(rwlock != nullptr);

    Require(pthread_rwlock_setname_np_nid_postfix(&rwlock, nullptr) == PosixEinval);
    Require(pthread_rwlock_setname_np_nid_postfix(&rwlock, "WorkerRwlock") == 0);
    Require(pthread_rwlock_setname_np_nid_postfix(&rwlock, "WorkerRwlockRenamed") == PosixEagain);

    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == 0);
    Require(ThrowsRuntimeError([&] { return pthread_rwlock_setname_np_nid_postfix(&rwlock, "AfterDestroy"); }));
}

void TestRwlockLockingRecursionAndUnlockErrors() {
    PthreadRwlock rwlock = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&rwlock, nullptr) == 0);
    Require(rwlock != nullptr);

    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == PosixEperm);
    Require(scePthreadRwlockUnlock(&rwlock) == SceKernelErrorEperm);

    Require(pthread_rwlock_wrlock_nid_postfix(&rwlock) == 0);

    Require(pthread_rwlock_tryrdlock_nid_postfix(&rwlock) == PosixEbusy);
    Require(pthread_rwlock_trywrlock_nid_postfix(&rwlock) == PosixEbusy);
    Require(scePthreadRwlockTryrdlock(&rwlock) == SceKernelErrorEbusy);
    Require(scePthreadRwlockTrywrlock(&rwlock) == SceKernelErrorEbusy);

    Require(scePthreadRwlockRdlock(&rwlock) == SceKernelErrorEdeadlk);
    Require(scePthreadRwlockWrlock(&rwlock) == SceKernelErrorEdeadlk);

    Require(scePthreadRwlockTimedrdlock(&rwlock, 1000) == SceKernelErrorEtimedout);
    Require(scePthreadRwlockTimedwrlock(&rwlock, 1000) == SceKernelErrorEtimedout);
    Require(pthread_rwlock_reltimedrdlock_np_nid_postfix(&rwlock, 1000) == PosixEtimedout);
    Require(pthread_rwlock_reltimedwrlock_np_nid_postfix(&rwlock, 1000) == PosixEtimedout);

    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == PosixEperm);

    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == 0);
}

void TestRwlockTimedWaitTimespecBounds() {
    PthreadRwlock rwlock = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&rwlock, nullptr) == 0);
    Require(rwlock != nullptr);

    Require(pthread_rwlock_timedrdlock_nid_postfix(&rwlock, nullptr) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);

    Require(pthread_rwlock_timedwrlock_nid_postfix(&rwlock, nullptr) == 0);

    const KernelTimespec invalidNanosNegative{0, -1};
    Require(pthread_rwlock_timedrdlock_nid_postfix(&rwlock, &invalidNanosNegative) == PosixEinval);
    Require(pthread_rwlock_timedwrlock_nid_postfix(&rwlock, &invalidNanosNegative) == PosixEinval);

    const KernelTimespec invalidNanosLimit{0, 1000000000};
    Require(pthread_rwlock_timedrdlock_nid_postfix(&rwlock, &invalidNanosLimit) == PosixEinval);
    Require(pthread_rwlock_timedwrlock_nid_postfix(&rwlock, &invalidNanosLimit) == PosixEinval);

    const KernelTimespec pastTime{1, 0};
    Require(pthread_rwlock_timedrdlock_nid_postfix(&rwlock, &pastTime) == PosixEtimedout);
    Require(pthread_rwlock_timedwrlock_nid_postfix(&rwlock, &pastTime) == PosixEtimedout);

    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);

    KernelTimespec now{};
    Require(clock_gettime_nid_postfix(0, &now) == 0);
    KernelTimespec futureTime{now.tv_sec + 2, now.tv_nsec};
    Require(pthread_rwlock_timedrdlock_nid_postfix(&rwlock, &futureTime) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);

    Require(pthread_rwlock_timedwrlock_nid_postfix(&rwlock, &futureTime) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);

    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == 0);
}

void TestRwlockThreadExclusionAndContention() {
    ThreadSyncContext sync;
    Require(pthread_rwlock_init_nid_postfix(&sync.rwlock, nullptr) == 0);
    Require(sync.rwlock != nullptr);

    Pthread worker = nullptr;
    Require(scePthreadCreate(&worker, nullptr, WriterWorker, &sync, nullptr) == 0);

    while (!sync.held.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    Require(pthread_rwlock_tryrdlock_nid_postfix(&sync.rwlock) == PosixEbusy);
    Require(pthread_rwlock_trywrlock_nid_postfix(&sync.rwlock) == PosixEbusy);
    Require(scePthreadRwlockTryrdlock(&sync.rwlock) == SceKernelErrorEbusy);
    Require(scePthreadRwlockTrywrlock(&sync.rwlock) == SceKernelErrorEbusy);

    Require(pthread_rwlock_reltimedrdlock_np_nid_postfix(&sync.rwlock, 1000) == PosixEtimedout);
    Require(pthread_rwlock_reltimedwrlock_np_nid_postfix(&sync.rwlock, 1000) == PosixEtimedout);
    Require(scePthreadRwlockTimedrdlock(&sync.rwlock, 1000) == SceKernelErrorEtimedout);
    Require(scePthreadRwlockTimedwrlock(&sync.rwlock, 1000) == SceKernelErrorEtimedout);

    sync.release.store(true, std::memory_order_release);
    Require(scePthreadJoin(worker, nullptr) == 0);

    Require(pthread_rwlock_trywrlock_nid_postfix(&sync.rwlock) == 0);
    Require(pthread_rwlock_unlock_nid_postfix(&sync.rwlock) == 0);

    MultiReaderContext readerSync;
    readerSync.rwlock = sync.rwlock;

    Pthread r1 = nullptr;
    Pthread r2 = nullptr;
    Require(scePthreadCreate(&r1, nullptr, ReaderWorker, &readerSync, nullptr) == 0);
    Require(scePthreadCreate(&r2, nullptr, ReaderWorker, &readerSync, nullptr) == 0);

    while (readerSync.readersEntered.load(std::memory_order_acquire) < 2) {
        std::this_thread::yield();
    }

    Require(pthread_rwlock_tryrdlock_nid_postfix(&readerSync.rwlock) == 0);
    Require(pthread_rwlock_trywrlock_nid_postfix(&readerSync.rwlock) == PosixEbusy);
    Require(pthread_rwlock_unlock_nid_postfix(&readerSync.rwlock) == 0);

    readerSync.release.store(true, std::memory_order_release);
    Require(scePthreadJoin(r1, nullptr) == 0);
    Require(scePthreadJoin(r2, nullptr) == 0);

    Require(pthread_rwlock_destroy_nid_postfix(&sync.rwlock) == 0);
}

} // namespace

int main() {
    TestRwlockattrLifecycleAndDefaults();
    TestRwlockattrTypeValidationAndBounds();
    TestRwlockattrPsharedValidationAndBounds();
    TestRwlockattrNullAndDestroyedPrecedence();
    TestRwlockInitAndDestroyLifecycle();
    TestRwlockSetNameBounds();
    TestRwlockLockingRecursionAndUnlockErrors();
    TestRwlockTimedWaitTimespecBounds();
    TestRwlockThreadExclusionAndContention();
    return 0;
}
