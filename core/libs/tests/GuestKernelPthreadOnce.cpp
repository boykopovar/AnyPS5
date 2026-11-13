#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <thread>

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

static constexpr unsigned THREAD_COUNT = 16;
static constexpr int POSIX_EINVAL = 22;

static void APS5_VABI DummyInit() {}

static int gSinglePosixCalls = 0;
static int gSinglePosixValue = 0;
static void APS5_VABI SinglePosixInit() {
    ++gSinglePosixCalls;
    gSinglePosixValue = 42;
}

static int gSecondaryPosixCalls = 0;
static void APS5_VABI SecondaryPosixInit() {
    ++gSecondaryPosixCalls;
}

static int gSingleSceCalls = 0;
static int gSingleSceValue = 0;
static void APS5_VABI SingleSceInit() {
    ++gSingleSceCalls;
    gSingleSceValue = 84;
}

static int gSecondarySceCalls = 0;
static void APS5_VABI SecondarySceInit() {
    ++gSecondarySceCalls;
}

static GuestOnce gConcurrentPosixControl{};
static std::atomic<int> gPosixStartGate{0};
static std::atomic<int> gPosixInitCalls{0};
static std::atomic<int> gPosixWorkerCompletions{0};
static int gPosixPublishedPayload = 0;

static void APS5_VABI ConcurrentPosixInit() {
    gPosixInitCalls.fetch_add(1, std::memory_order_relaxed);
    gPosixPublishedPayload = 0x12345678;
    std::this_thread::yield();
}

static void* APS5_VABI PosixConcurrencyWorker(void*) {
    gPosixStartGate.fetch_add(1, std::memory_order_release);
    while (gPosixStartGate.load(std::memory_order_acquire) < static_cast<int>(THREAD_COUNT)) {
        std::this_thread::yield();
    }
    const int rc = pthread_once_nid_postfix(&gConcurrentPosixControl, ConcurrentPosixInit);
    Require(rc == 0);
    Require(gPosixPublishedPayload == 0x12345678);
    gPosixWorkerCompletions.fetch_add(1, std::memory_order_release);
    return nullptr;
}

static int32_t gConcurrentSceControl = 0;
static std::atomic<int> gSceStartGate{0};
static std::atomic<int> gSceInitCalls{0};
static std::atomic<int> gSceWorkerCompletions{0};
static int gScePublishedPayload = 0;

static void APS5_VABI ConcurrentSceInit() {
    gSceInitCalls.fetch_add(1, std::memory_order_relaxed);
    gScePublishedPayload = 0x87654321;
    std::this_thread::yield();
}

static void* APS5_VABI SceConcurrencyWorker(void*) {
    gSceStartGate.fetch_add(1, std::memory_order_release);
    while (gSceStartGate.load(std::memory_order_acquire) < static_cast<int>(THREAD_COUNT)) {
        std::this_thread::yield();
    }
    const int rc = scePthreadOnce(&gConcurrentSceControl, ConcurrentSceInit);
    Require(rc == 0);
    Require(gScePublishedPayload == 0x87654321);
    gSceWorkerCompletions.fetch_add(1, std::memory_order_release);
    return nullptr;
}

static GuestOnce gOuterControl{};
static GuestOnce gInnerControl{};
static int gOuterValue = 0;
static int gInnerValue = 0;

static void APS5_VABI InnerInit() {
    gInnerValue = 777;
}

static void APS5_VABI OuterInit() {
    gOuterValue = 333;
    const int rc = pthread_once_nid_postfix(&gInnerControl, InnerInit);
    Require(rc == 0);
}

static GuestOnce gRetryControl{};
static int gRetryAttempts = 0;
static int gRetryFinalPayload = 0;

static void APS5_VABI RetryInit() {
    ++gRetryAttempts;
    if (gRetryAttempts == 1) {
        throw 999;
    }
    gRetryFinalPayload = 555;
}

static void TestInvalidArguments() {
    Require(pthread_once_nid_postfix(nullptr, DummyInit) == POSIX_EINVAL);
    GuestOnce validCtrl{};
    Require(pthread_once_nid_postfix(&validCtrl, nullptr) == POSIX_EINVAL);
    Require(pthread_once_nid_postfix(nullptr, nullptr) == POSIX_EINVAL);

    GuestOnce invalidState3{3, nullptr};
    Require(pthread_once_nid_postfix(&invalidState3, DummyInit) == POSIX_EINVAL);

    GuestOnce invalidState42{42, nullptr};
    Require(pthread_once_nid_postfix(&invalidState42, DummyInit) == POSIX_EINVAL);

    GuestOnce invalidStateNeg{-1, nullptr};
    Require(pthread_once_nid_postfix(&invalidStateNeg, DummyInit) == POSIX_EINVAL);

    GuestOnce invalidStateLarge{9999, nullptr};
    Require(pthread_once_nid_postfix(&invalidStateLarge, DummyInit) == POSIX_EINVAL);

    Require(scePthreadOnce(nullptr, DummyInit) == SCE_KERNEL_ERROR_EINVAL);
    int32_t validSce = 0;
    Require(scePthreadOnce(&validSce, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadOnce(nullptr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
}

static void TestSingleThreadExecution() {
    GuestOnce posixCtrl{};
    Require(posixCtrl.state == 0);
    Require(pthread_once_nid_postfix(&posixCtrl, SinglePosixInit) == 0);
    Require(gSinglePosixCalls == 1);
    Require(gSinglePosixValue == 42);
    Require(posixCtrl.state == 1);

    Require(pthread_once_nid_postfix(&posixCtrl, SinglePosixInit) == 0);
    Require(gSinglePosixCalls == 1);

    Require(pthread_once_nid_postfix(&posixCtrl, SecondaryPosixInit) == 0);
    Require(gSecondaryPosixCalls == 0);

    int32_t sceCtrl = 0;
    Require(scePthreadOnce(&sceCtrl, SingleSceInit) == 0);
    Require(gSingleSceCalls == 1);
    Require(gSingleSceValue == 84);
    Require(sceCtrl == 1);

    Require(scePthreadOnce(&sceCtrl, SingleSceInit) == 0);
    Require(gSingleSceCalls == 1);

    Require(scePthreadOnce(&sceCtrl, SecondarySceInit) == 0);
    Require(gSecondarySceCalls == 0);
}

static void TestPosixConcurrency() {
    std::array<Pthread, THREAD_COUNT> threads{};
    for (auto& thread : threads) {
        Require(scePthreadCreate(&thread, nullptr, PosixConcurrencyWorker, nullptr, "posix_once") == 0);
    }
    for (auto& thread : threads) {
        Require(scePthreadJoin(thread, nullptr) == 0);
    }
    Require(gPosixInitCalls.load(std::memory_order_acquire) == 1);
    Require(gPosixWorkerCompletions.load(std::memory_order_acquire) == static_cast<int>(THREAD_COUNT));
    Require(gConcurrentPosixControl.state == 1);
    Require(pthread_once_nid_postfix(&gConcurrentPosixControl, ConcurrentPosixInit) == 0);
    Require(gPosixInitCalls.load(std::memory_order_acquire) == 1);
}

static void TestSceConcurrency() {
    std::array<Pthread, THREAD_COUNT> threads{};
    for (auto& thread : threads) {
        Require(scePthreadCreate(&thread, nullptr, SceConcurrencyWorker, nullptr, "sce_once") == 0);
    }
    for (auto& thread : threads) {
        Require(scePthreadJoin(thread, nullptr) == 0);
    }
    Require(gSceInitCalls.load(std::memory_order_acquire) == 1);
    Require(gSceWorkerCompletions.load(std::memory_order_acquire) == static_cast<int>(THREAD_COUNT));
    Require(gConcurrentSceControl == 1);
    Require(scePthreadOnce(&gConcurrentSceControl, ConcurrentSceInit) == 0);
    Require(gSceInitCalls.load(std::memory_order_acquire) == 1);
}

static void TestNestedOnce() {
    Require(pthread_once_nid_postfix(&gOuterControl, OuterInit) == 0);
    Require(gOuterValue == 333);
    Require(gInnerValue == 777);
    Require(gOuterControl.state == 1);
    Require(gInnerControl.state == 1);
}

static void TestExceptionRetry() {
    Require(gRetryControl.state == 0);
    bool caughtException = false;
    try {
        pthread_once_nid_postfix(&gRetryControl, RetryInit);
    } catch (int val) {
        caughtException = (val == 999);
    }
    Require(caughtException);
    Require(gRetryAttempts == 1);
    Require(gRetryControl.state == 0);

    Require(pthread_once_nid_postfix(&gRetryControl, RetryInit) == 0);
    Require(gRetryAttempts == 2);
    Require(gRetryFinalPayload == 555);
    Require(gRetryControl.state == 1);
}

int main() {
    TestInvalidArguments();
    TestSingleThreadExecution();
    TestPosixConcurrency();
    TestSceConcurrency();
    TestNestedOnce();
    TestExceptionRetry();
    return 0;
}
