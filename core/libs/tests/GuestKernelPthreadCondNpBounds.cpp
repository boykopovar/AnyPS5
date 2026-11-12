#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <thread>

namespace {

constexpr int GuestEperm = 1;
constexpr int GuestEinval = 22;
constexpr int GuestEtimedout = 60;
constexpr int MutexTypeRecursive = 2;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

template <typename TFunc>
bool ThrowsInvalidArgument(TFunc func) {
    try {
        func();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

struct WaitSync {
    PthreadMutex mutex = nullptr;
    PthreadCond cond = nullptr;
    std::atomic<bool> waiting{false};
    std::atomic<bool> predicate{false};
    std::atomic<int> result{-1};
};

void* APS5_VABI SignaledWorker(void* arg) {
    auto& sync = *static_cast<WaitSync*>(arg);
    Require(pthread_mutex_lock_nid_postfix(&sync.mutex) == 0);
    sync.waiting.store(true);
    sync.result.store(pthread_cond_reltimedwait_np_nid_postfix(&sync.cond, &sync.mutex, 5000000));
    Require(sync.predicate.load());
    Require(pthread_mutex_unlock_nid_postfix(&sync.mutex) == 0);
    return nullptr;
}

struct BroadcastSync {
    PthreadMutex mutex = nullptr;
    PthreadCond cond = nullptr;
    std::atomic<int> waitCount{0};
    std::atomic<bool> released{false};
    std::atomic<int> result1{-1};
    std::atomic<int> result2{-1};
};

void* APS5_VABI BroadcastWorker1(void* arg) {
    auto& sync = *static_cast<BroadcastSync*>(arg);
    Require(pthread_mutex_lock_nid_postfix(&sync.mutex) == 0);
    sync.waitCount.fetch_add(1);
    sync.result1.store(pthread_cond_reltimedwait_np_nid_postfix(&sync.cond, &sync.mutex, 5000000));
    Require(sync.released.load());
    Require(pthread_mutex_unlock_nid_postfix(&sync.mutex) == 0);
    return nullptr;
}

void* APS5_VABI BroadcastWorker2(void* arg) {
    auto& sync = *static_cast<BroadcastSync*>(arg);
    Require(pthread_mutex_lock_nid_postfix(&sync.mutex) == 0);
    sync.waitCount.fetch_add(1);
    sync.result2.store(pthread_cond_reltimedwait_np_nid_postfix(&sync.cond, &sync.mutex, 5000000));
    Require(sync.released.load());
    Require(pthread_mutex_unlock_nid_postfix(&sync.mutex) == 0);
    return nullptr;
}

struct ForeignHolder {
    PthreadMutex mutex = nullptr;
    std::atomic<bool> locked{false};
    std::atomic<bool> release{false};
};

void* APS5_VABI ForeignLockWorker(void* arg) {
    auto& holder = *static_cast<ForeignHolder*>(arg);
    Require(pthread_mutex_lock_nid_postfix(&holder.mutex) == 0);
    holder.locked.store(true);
    while (!holder.release.load()) {
        std::this_thread::yield();
    }
    Require(pthread_mutex_unlock_nid_postfix(&holder.mutex) == 0);
    return nullptr;
}

void TestInvalidPointersAndStates() {
    PthreadCond cond = nullptr;
    Require(pthread_cond_init_nid_postfix(&cond, nullptr) == 0);

    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);

    Require(pthread_cond_reltimedwait_np_nid_postfix(nullptr, &mutex, 1000) == GuestEinval);
    Require(pthread_cond_reltimedwait_np_nid_postfix(&cond, nullptr, 1000) == GuestEinval);
    Require(pthread_cond_reltimedwait_np_nid_postfix(nullptr, nullptr, 1000) == GuestEinval);

    Require(pthread_cond_reltimedwait_np_nid_postfix(&cond, &mutex, 1000) == GuestEperm);

    ForeignHolder holder;
    Require(pthread_mutex_init_nid_postfix(&holder.mutex, nullptr) == 0);
    Pthread holderThread = nullptr;
    Require(pthread_create_nid_postfix(&holderThread, nullptr, ForeignLockWorker, &holder) == 0);
    while (!holder.locked.load()) {
        std::this_thread::yield();
    }
    Require(pthread_cond_reltimedwait_np_nid_postfix(&cond, &holder.mutex, 1000) == GuestEperm);
    holder.release.store(true);
    Require(pthread_join_nid_postfix(holderThread, nullptr) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&holder.mutex) == 0);

    PthreadMutex deadMutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&deadMutex, nullptr) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&deadMutex) == 0);
    Require(pthread_cond_reltimedwait_np_nid_postfix(&cond, &deadMutex, 1000) == GuestEinval);

    PthreadCond deadCond = nullptr;
    Require(pthread_cond_init_nid_postfix(&deadCond, nullptr) == 0);
    Require(pthread_cond_destroy_nid_postfix(&deadCond) == 0);
    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_cond_reltimedwait_np_nid_postfix(&deadCond, &mutex, 1000) == GuestEinval);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);

    Require(pthread_cond_destroy_nid_postfix(&cond) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
}

void TestRelativeTimeoutZero() {
    PthreadCond cond = nullptr;
    Require(pthread_cond_init_nid_postfix(&cond, nullptr) == 0);

    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);

    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_cond_reltimedwait_np_nid_postfix(&cond, &mutex, 0) == GuestEtimedout);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);

    Require(pthread_cond_destroy_nid_postfix(&cond) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
}

void TestRelativeTimeoutExpiry() {
    PthreadCond cond = nullptr;
    Require(pthread_cond_init_nid_postfix(&cond, nullptr) == 0);

    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);

    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    const auto before = std::chrono::steady_clock::now();
    Require(pthread_cond_reltimedwait_np_nid_postfix(&cond, &mutex, 10000) == GuestEtimedout);
    const auto elapsed = std::chrono::steady_clock::now() - before;
    Require(elapsed >= std::chrono::microseconds(5000));
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);

    Require(pthread_cond_destroy_nid_postfix(&cond) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
}

void TestLazyInitialization() {
    PthreadCond lazy = nullptr;
    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);

    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_cond_reltimedwait_np_nid_postfix(&lazy, &mutex, 0) == GuestEtimedout);
    Require(lazy != nullptr);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);

    Require(pthread_cond_destroy_nid_postfix(&lazy) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
}

void TestSignaledWakeup() {
    WaitSync sync;
    Require(pthread_cond_init_nid_postfix(&sync.cond, nullptr) == 0);
    Require(pthread_mutex_init_nid_postfix(&sync.mutex, nullptr) == 0);

    Pthread thread = nullptr;
    Require(pthread_create_nid_postfix(&thread, nullptr, SignaledWorker, &sync) == 0);

    while (!sync.waiting.load()) {
        std::this_thread::yield();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    Require(pthread_mutex_lock_nid_postfix(&sync.mutex) == 0);
    sync.predicate.store(true);
    Require(pthread_cond_signal_nid_postfix(&sync.cond) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&sync.mutex) == 0);

    Require(pthread_join_nid_postfix(thread, nullptr) == 0);
    Require(sync.result.load() == 0);

    Require(pthread_cond_destroy_nid_postfix(&sync.cond) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&sync.mutex) == 0);
}

void TestBroadcastWakeup() {
    BroadcastSync sync;
    Require(pthread_cond_init_nid_postfix(&sync.cond, nullptr) == 0);
    Require(pthread_mutex_init_nid_postfix(&sync.mutex, nullptr) == 0);

    Pthread t1 = nullptr;
    Pthread t2 = nullptr;
    Require(pthread_create_nid_postfix(&t1, nullptr, BroadcastWorker1, &sync) == 0);
    Require(pthread_create_nid_postfix(&t2, nullptr, BroadcastWorker2, &sync) == 0);

    while (sync.waitCount.load() < 2) {
        std::this_thread::yield();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    Require(pthread_mutex_lock_nid_postfix(&sync.mutex) == 0);
    sync.released.store(true);
    Require(pthread_cond_broadcast_nid_postfix(&sync.cond) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&sync.mutex) == 0);

    Require(pthread_join_nid_postfix(t1, nullptr) == 0);
    Require(pthread_join_nid_postfix(t2, nullptr) == 0);
    Require(sync.result1.load() == 0);
    Require(sync.result2.load() == 0);

    Require(pthread_cond_destroy_nid_postfix(&sync.cond) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&sync.mutex) == 0);
}

void TestRecursiveMutexPreservation() {
    PthreadMutexattr attr = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_settype_nid_postfix(&attr, MutexTypeRecursive) == 0);

    PthreadMutex recMutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&recMutex, &attr) == 0);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);

    PthreadCond cond = nullptr;
    Require(pthread_cond_init_nid_postfix(&cond, nullptr) == 0);

    Require(pthread_mutex_lock_nid_postfix(&recMutex) == 0);
    Require(pthread_mutex_lock_nid_postfix(&recMutex) == 0);

    Require(pthread_cond_reltimedwait_np_nid_postfix(&cond, &recMutex, 0) == GuestEtimedout);

    Require(pthread_mutex_unlock_nid_postfix(&recMutex) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&recMutex) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&recMutex) == GuestEperm);

    Require(pthread_cond_destroy_nid_postfix(&cond) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&recMutex) == 0);
}

void TestCondSetnameNpBounds() {
    Require(pthread_cond_setname_np_nid_postfix(nullptr, "WorkerCond") == GuestEinval);
    Require(pthread_cond_setname_np_nid_postfix(nullptr, nullptr) == GuestEinval);

    PthreadCond cond = nullptr;
    Require(pthread_cond_init_nid_postfix(&cond, nullptr) == 0);
    Require(pthread_cond_setname_np_nid_postfix(&cond, "Worker118Cond") == 0);
    Require(ThrowsInvalidArgument([&] { return pthread_cond_setname_np_nid_postfix(&cond, nullptr); }));

    Require(pthread_cond_destroy_nid_postfix(&cond) == 0);
    Require(pthread_cond_setname_np_nid_postfix(&cond, "PostDestroy") == GuestEinval);

    PthreadCond lazy = nullptr;
    Require(pthread_cond_setname_np_nid_postfix(&lazy, "LazyCond") == 0);
    Require(lazy != nullptr);
    Require(pthread_cond_destroy_nid_postfix(&lazy) == 0);
}

} // namespace

int main() {
    TestInvalidPointersAndStates();
    TestRelativeTimeoutZero();
    TestRelativeTimeoutExpiry();
    TestLazyInitialization();
    TestSignaledWakeup();
    TestBroadcastWakeup();
    TestRecursiveMutexPreservation();
    TestCondSetnameNpBounds();
    std::puts("GuestKernelPthreadCondNpBounds passed");
    return 0;
}
