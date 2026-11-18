#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <thread>

namespace {

constexpr int PosixEperm = 1;
constexpr int PosixEsrch = 3;
constexpr int PosixEinval = 22;
constexpr int PosixEtimedout = 60;

constexpr int MutexTypeRecursive = 2;
constexpr KernelClockid ClockRealtime = 0;
constexpr KernelClockid ClockMonotonic = 4;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename Function>
bool ThrowsInvalidArgument(Function func) {
    try {
        func();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void TestSceCondInitAndDestroyBounds() {
    Require(ThrowsInvalidArgument([] { (void)scePthreadCondInit(nullptr, nullptr, nullptr); }));
    Require(ThrowsInvalidArgument([] { (void)scePthreadCondDestroy(nullptr); }));

    PthreadCond uninit = nullptr;
    Require(scePthreadCondDestroy(&uninit) == 0);

    PthreadCond cond = nullptr;
    Require(scePthreadCondInit(&cond, nullptr, "SampleCond") == 0);
    Require(cond != nullptr);
    Require(scePthreadCondDestroy(&cond) == 0);
    Require(scePthreadCondDestroy(&cond) == SCE_KERNEL_ERROR_EINVAL);

    PthreadCondattr attr = nullptr;
    Require(scePthreadCondattrInit(&attr) == 0);
    Require(scePthreadCondattrSetclock(&attr, ClockMonotonic) == 0);

    PthreadCond condMonotonic = nullptr;
    Require(scePthreadCondInit(&condMonotonic, &attr, "MonotonicCond") == 0);
    Require(condMonotonic != nullptr);
    Require(scePthreadCondDestroy(&condMonotonic) == 0);
    Require(scePthreadCondattrDestroy(&attr) == 0);
}

void TestPosixCondInitAndDestroyBounds() {
    Require(pthread_cond_init_nid_postfix(nullptr, nullptr) == PosixEinval);
    Require(pthread_cond_destroy_nid_postfix(nullptr) == PosixEinval);

    PthreadCond uninit = nullptr;
    Require(pthread_cond_destroy_nid_postfix(&uninit) == 0);

    PthreadCond cond = nullptr;
    Require(pthread_cond_init_nid_postfix(&cond, nullptr) == 0);
    Require(cond != nullptr);
    Require(pthread_cond_destroy_nid_postfix(&cond) == 0);
    Require(pthread_cond_destroy_nid_postfix(&cond) == PosixEinval);
}

void TestSignalAndBroadcastBounds() {
    Require(pthread_cond_signal_nid_postfix(nullptr) == PosixEinval);
    Require(pthread_cond_broadcast_nid_postfix(nullptr) == PosixEinval);

    PthreadCond destroyed = nullptr;
    Require(scePthreadCondInit(&destroyed, nullptr, nullptr) == 0);
    Require(scePthreadCondDestroy(&destroyed) == 0);
    Require(scePthreadCondSignal(&destroyed) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadCondBroadcast(&destroyed) == SCE_KERNEL_ERROR_EINVAL);
    Require(pthread_cond_signal_nid_postfix(&destroyed) == PosixEinval);
    Require(pthread_cond_broadcast_nid_postfix(&destroyed) == PosixEinval);

    PthreadCond cond = nullptr;
    Require(scePthreadCondInit(&cond, nullptr, nullptr) == 0);
    Require(scePthreadCondSignal(&cond) == 0);
    Require(scePthreadCondBroadcast(&cond) == 0);
    Require(pthread_cond_signal_nid_postfix(&cond) == 0);
    Require(pthread_cond_broadcast_nid_postfix(&cond) == 0);
    Require(scePthreadCondDestroy(&cond) == 0);

    PthreadCond lazySignal = nullptr;
    Require(scePthreadCondSignal(&lazySignal) == 0);
    Require(lazySignal != nullptr);
    Require(scePthreadCondDestroy(&lazySignal) == 0);

    PthreadCond lazyBroadcast = nullptr;
    Require(scePthreadCondBroadcast(&lazyBroadcast) == 0);
    Require(lazyBroadcast != nullptr);
    Require(scePthreadCondDestroy(&lazyBroadcast) == 0);
}

void TestSignaltoTargetBounds() {
    PthreadCond cond = nullptr;
    Require(scePthreadCondInit(&cond, nullptr, nullptr) == 0);

    Require(pthread_cond_signalto_np_nid_postfix(nullptr, nullptr) == PosixEinval);
    Require(pthread_cond_signalto_np_nid_postfix(&cond, nullptr) == PosixEinval);
    Require(pthread_cond_signalto_np_nid_postfix(nullptr, reinterpret_cast<Pthread>(1)) == PosixEinval);

    Require(scePthreadCondSignalto(nullptr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadCondSignalto(&cond, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadCondSignalto(nullptr, reinterpret_cast<Pthread>(1)) == SCE_KERNEL_ERROR_EINVAL);

    const auto invalidThread = reinterpret_cast<Pthread>(0x12345678);
    Require(scePthreadCondSignalto(&cond, invalidThread) == SCE_KERNEL_ERROR_ESRCH);
    Require(pthread_cond_signalto_np_nid_postfix(&cond, invalidThread) == PosixEsrch);

    const Pthread self = scePthreadSelf();
    Require(scePthreadCondSignalto(&cond, self) == SCE_KERNEL_ERROR_EPERM);
    Require(pthread_cond_signalto_np_nid_postfix(&cond, self) == PosixEperm);

    Require(scePthreadCondDestroy(&cond) == 0);

    Require(scePthreadCondSignalto(&cond, self) == SCE_KERNEL_ERROR_EINVAL);
    Require(pthread_cond_signalto_np_nid_postfix(&cond, self) == PosixEinval);
}

void TestWaitAndTimedwaitPrecedence() {
    PthreadCond cond = nullptr;
    Require(scePthreadCondInit(&cond, nullptr, nullptr) == 0);

    PthreadMutex mutex = nullptr;
    Require(scePthreadMutexInit(&mutex, nullptr, nullptr) == 0);

    Require(ThrowsInvalidArgument([&] { (void)scePthreadCondWait(nullptr, &mutex); }));
    Require(ThrowsInvalidArgument([&] { (void)scePthreadCondWait(&cond, nullptr); }));
    Require(pthread_cond_wait_nid_postfix(nullptr, &mutex) == PosixEinval);
    Require(pthread_cond_wait_nid_postfix(&cond, nullptr) == PosixEinval);

    Require(ThrowsInvalidArgument([&] { (void)scePthreadCondTimedwait(nullptr, &mutex, 100); }));
    Require(ThrowsInvalidArgument([&] { (void)scePthreadCondTimedwait(&cond, nullptr, 100); }));
    Require(pthread_cond_reltimedwait_np_nid_postfix(nullptr, &mutex, 100) == PosixEinval);
    Require(pthread_cond_reltimedwait_np_nid_postfix(&cond, nullptr, 100) == PosixEinval);

    const KernelTimespec validPast{0, 0};
    Require(pthread_cond_timedwait_nid_postfix(nullptr, &mutex, &validPast) == PosixEinval);
    Require(pthread_cond_timedwait_nid_postfix(&cond, nullptr, &validPast) == PosixEinval);
    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, nullptr) == PosixEinval);

    const KernelTimespec invalidTimespecs[] = {
        {-1, 0},
        {-50, 200},
        {0, -1},
        {10, -500},
        {0, 1000000000},
        {5, 1000000001},
        {1, 2000000000}
    };
    for (const auto& ts : invalidTimespecs) {
        Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &ts) == PosixEinval);
    }

    Require(scePthreadCondWait(&cond, &mutex) == SCE_KERNEL_ERROR_EPERM);
    Require(scePthreadCondTimedwait(&cond, &mutex, 100) == SCE_KERNEL_ERROR_EPERM);
    Require(pthread_cond_wait_nid_postfix(&cond, &mutex) == PosixEperm);
    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &validPast) == PosixEperm);
    Require(pthread_cond_reltimedwait_np_nid_postfix(&cond, &mutex, 100) == PosixEperm);

    PthreadMutex deadMutex = nullptr;
    Require(scePthreadMutexInit(&deadMutex, nullptr, nullptr) == 0);
    Require(scePthreadMutexDestroy(&deadMutex) == 0);

    Require(scePthreadCondWait(&cond, &deadMutex) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadCondTimedwait(&cond, &deadMutex, 100) == SCE_KERNEL_ERROR_EINVAL);
    Require(pthread_cond_wait_nid_postfix(&cond, &deadMutex) == PosixEinval);
    Require(pthread_cond_timedwait_nid_postfix(&cond, &deadMutex, &validPast) == PosixEinval);
    Require(pthread_cond_reltimedwait_np_nid_postfix(&cond, &deadMutex, 100) == PosixEinval);

    PthreadCond deadCond = nullptr;
    Require(scePthreadCondInit(&deadCond, nullptr, nullptr) == 0);
    Require(scePthreadCondDestroy(&deadCond) == 0);

    Require(scePthreadMutexLock(&mutex) == 0);
    Require(scePthreadCondWait(&deadCond, &mutex) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadCondTimedwait(&deadCond, &mutex, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(pthread_cond_wait_nid_postfix(&deadCond, &mutex) == PosixEinval);
    Require(pthread_cond_timedwait_nid_postfix(&deadCond, &mutex, &validPast) == PosixEinval);
    Require(pthread_cond_reltimedwait_np_nid_postfix(&deadCond, &mutex, 0) == PosixEinval);
    Require(scePthreadMutexUnlock(&mutex) == 0);

    Require(scePthreadCondDestroy(&cond) == 0);
    Require(scePthreadMutexDestroy(&mutex) == 0);
}

void TestSceTimedwaitExpiryAndOwnership() {
    PthreadMutex mutex = nullptr;
    PthreadCond cond = nullptr;
    Require(scePthreadMutexInit(&mutex, nullptr, nullptr) == 0);
    Require(scePthreadCondInit(&cond, nullptr, nullptr) == 0);

    Require(scePthreadMutexLock(&mutex) == 0);

    Require(scePthreadCondTimedwait(&cond, &mutex, 0) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(scePthreadMutexUnlock(&mutex) == 0);

    Require(scePthreadMutexLock(&mutex) == 0);
    const auto start = std::chrono::steady_clock::now();
    Require(scePthreadCondTimedwait(&cond, &mutex, 20000) == SCE_KERNEL_ERROR_ETIMEDOUT);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    Require(elapsed >= std::chrono::milliseconds(10));
    Require(scePthreadMutexUnlock(&mutex) == 0);

    Require(scePthreadCondDestroy(&cond) == 0);
    Require(scePthreadMutexDestroy(&mutex) == 0);
}

struct SignalSync {
    PthreadMutex mutex = nullptr;
    PthreadCond cond = nullptr;
    std::atomic<int> waiting{0};
    std::atomic<int> woken{0};
    std::atomic<bool> allowExit{false};
};

void* APS5_VABI SignalWorker(void* arg) {
    auto& sync = *static_cast<SignalSync*>(arg);
    Require(scePthreadMutexLock(&sync.mutex) == 0);
    sync.waiting.fetch_add(1);
    while (!sync.allowExit.load()) {
        Require(scePthreadCondWait(&sync.cond, &sync.mutex) == 0);
        sync.woken.fetch_add(1);
        if (sync.allowExit.load()) {
            break;
        }
    }
    Require(scePthreadMutexUnlock(&sync.mutex) == 0);
    return nullptr;
}

void TestSignalWakeupSequence() {
    SignalSync sync;
    Require(scePthreadMutexInit(&sync.mutex, nullptr, nullptr) == 0);
    Require(scePthreadCondInit(&sync.cond, nullptr, nullptr) == 0);

    Pthread worker1 = nullptr;
    Pthread worker2 = nullptr;
    Require(scePthreadCreate(&worker1, nullptr, SignalWorker, &sync, "worker1") == 0);
    Require(scePthreadCreate(&worker2, nullptr, SignalWorker, &sync, "worker2") == 0);

    while (sync.waiting.load() < 2) {
        std::this_thread::yield();
    }

    Require(scePthreadMutexLock(&sync.mutex) == 0);
    Require(scePthreadCondSignal(&sync.cond) == 0);
    Require(scePthreadMutexUnlock(&sync.mutex) == 0);

    while (sync.woken.load() < 1) {
        std::this_thread::yield();
    }

    Require(scePthreadMutexLock(&sync.mutex) == 0);
    sync.allowExit.store(true);
    Require(scePthreadCondBroadcast(&sync.cond) == 0);
    Require(scePthreadMutexUnlock(&sync.mutex) == 0);

    void* ret1 = nullptr;
    void* ret2 = nullptr;
    Require(scePthreadJoin(worker1, &ret1) == 0);
    Require(scePthreadJoin(worker2, &ret2) == 0);
    Require(sync.woken.load() >= 2);

    Require(scePthreadCondDestroy(&sync.cond) == 0);
    Require(scePthreadMutexDestroy(&sync.mutex) == 0);
}

struct TargetSync {
    PthreadMutex mutex = nullptr;
    PthreadCond cond = nullptr;
    std::atomic<bool> waiting{false};
    std::atomic<bool> woken{false};
};

void* APS5_VABI TargetWorker(void* arg) {
    auto& sync = *static_cast<TargetSync*>(arg);
    Require(scePthreadMutexLock(&sync.mutex) == 0);
    sync.waiting.store(true);
    Require(scePthreadCondWait(&sync.cond, &sync.mutex) == 0);
    sync.woken.store(true);
    Require(scePthreadMutexUnlock(&sync.mutex) == 0);
    return nullptr;
}

void TestSignaltoWorkerWakeup() {
    TargetSync sync;
    Require(scePthreadMutexInit(&sync.mutex, nullptr, nullptr) == 0);
    Require(scePthreadCondInit(&sync.cond, nullptr, nullptr) == 0);

    Pthread worker = nullptr;
    Require(scePthreadCreate(&worker, nullptr, TargetWorker, &sync, "target_worker") == 0);

    while (!sync.waiting.load()) {
        std::this_thread::yield();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    Require(scePthreadCondSignalto(&sync.cond, worker) == 0);

    void* ret = nullptr;
    Require(scePthreadJoin(worker, &ret) == 0);
    Require(sync.woken.load());

    Require(scePthreadCondDestroy(&sync.cond) == 0);
    Require(scePthreadMutexDestroy(&sync.mutex) == 0);
}

void TestRecursiveMutexCountPreservation() {
    PthreadMutexattr attr = nullptr;
    Require(scePthreadMutexattrInit(&attr) == 0);
    Require(scePthreadMutexattrSettype(&attr, MutexTypeRecursive) == 0);

    PthreadMutex recMutex = nullptr;
    Require(scePthreadMutexInit(&recMutex, &attr, nullptr) == 0);
    Require(scePthreadMutexattrDestroy(&attr) == 0);

    PthreadCond cond = nullptr;
    Require(scePthreadCondInit(&cond, nullptr, nullptr) == 0);

    Require(scePthreadMutexLock(&recMutex) == 0);
    Require(scePthreadMutexLock(&recMutex) == 0);
    Require(scePthreadMutexLock(&recMutex) == 0);

    Require(scePthreadCondTimedwait(&cond, &recMutex, 0) == SCE_KERNEL_ERROR_ETIMEDOUT);

    Require(scePthreadMutexUnlock(&recMutex) == 0);
    Require(scePthreadMutexUnlock(&recMutex) == 0);
    Require(scePthreadMutexUnlock(&recMutex) == 0);
    Require(scePthreadMutexUnlock(&recMutex) == SCE_KERNEL_ERROR_EPERM);

    Require(scePthreadCondDestroy(&cond) == 0);
    Require(scePthreadMutexDestroy(&recMutex) == 0);
}

void TestCondSetNameBounds() {
    PthreadCond cond = nullptr;
    Require(scePthreadCondInit(&cond, nullptr, nullptr) == 0);

    Require(pthread_cond_setname_np_nid_postfix(nullptr, "ValidName") == PosixEinval);
    Require(pthread_cond_setname_np_nid_postfix(&cond, "ValidName") == 0);
    Require(ThrowsInvalidArgument([&] { (void)pthread_cond_setname_np_nid_postfix(&cond, nullptr); }));

    Require(pthread_cond_destroy_nid_postfix(&cond) == 0);
    Require(pthread_cond_setname_np_nid_postfix(&cond, "AfterDestroy") == PosixEinval);
}

} // namespace

int main() {
    TestSceCondInitAndDestroyBounds();
    TestPosixCondInitAndDestroyBounds();
    TestSignalAndBroadcastBounds();
    TestSignaltoTargetBounds();
    TestWaitAndTimedwaitPrecedence();
    TestSceTimedwaitExpiryAndOwnership();
    TestSignalWakeupSequence();
    TestSignaltoWorkerWakeup();
    TestRecursiveMutexCountPreservation();
    TestCondSetNameBounds();
    return 0;
}
