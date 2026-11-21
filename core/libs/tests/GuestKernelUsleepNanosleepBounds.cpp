#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <thread>

static constexpr int SCE_OK = 0;
static constexpr int GUEST_EFAULT = 14;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_EACCES = 13;
static constexpr int GUEST_SIGUSR1 = 30;
static constexpr std::int64_t NANOS_PER_SECOND = 1000000000LL;

static void Require(bool value) {
    if (!value) std::abort();
}

static std::atomic<int> handlerDelivered{0};
static std::atomic<int> lastSignal{0};

static void APS5_VABI SignalHandler(int signum, void*) {
    lastSignal.store(signum);
    handlerDelivered.fetch_add(1);
    Require(sceKernelUsleep_nid_postfix(50) == SCE_OK);
}

template <typename Predicate>
static bool WaitFor(Predicate predicate, std::chrono::milliseconds limit) {
    const auto deadline = std::chrono::steady_clock::now() + limit;
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}

static void TestUsleepSubMicrosecondBounds() {
    Require(sceKernelUsleep_nid_postfix(0) == SCE_OK);
    Require(sceKernelUsleep_nid_postfix(1) == SCE_OK);
    Require(sceKernelUsleep_nid_postfix(10) == SCE_OK);
    Require(sceKernelUsleep_nid_postfix(50) == SCE_OK);
    Require(sceKernelUsleep_nid_postfix(100) == SCE_OK);
    Require(sceKernelUsleep_nid_postfix(500) == SCE_OK);
    Require(sceKernelUsleep_nid_postfix(1000) == SCE_OK);
}

static void TestNanosleepSubBounds() {
    using PosixNanosleep = int (APS5_VABI *)(const KernelTimespec*, KernelTimespec*);
    const PosixNanosleep posixCalls[] = {nanosleep_nid_postfix, _nanosleep_nid_postfix};

    for (auto call : posixCalls) {
        KernelTimespec remaining{7, 9};
        *__error_nid_postfix() = GUEST_EACCES;
        const KernelTimespec zero{0, 0};
        Require(call(&zero, &remaining) == SCE_OK);
        Require(*__error_nid_postfix() == GUEST_EACCES);
        Require(remaining.tv_sec == 7 && remaining.tv_nsec == 9);

        const KernelTimespec subMicros[] = {
            {0, 1},
            {0, 10},
            {0, 100},
            {0, 500},
            {0, 999},
            {0, 1000},
            {0, 50000}
        };

        for (const auto& request : subMicros) {
            remaining = {13, 17};
            *__error_nid_postfix() = GUEST_EACCES;
            Require(call(&request, &remaining) == SCE_OK);
            Require(*__error_nid_postfix() == GUEST_EACCES);
            Require(remaining.tv_sec == 13 && remaining.tv_nsec == 17);
            Require(call(&request, nullptr) == SCE_OK);
        }
    }

    KernelTimespec sceRemaining{7, 9};
    const KernelTimespec sceZero{0, 0};
    Require(sceKernelNanosleep(&sceZero, &sceRemaining) == SCE_OK);
    Require(sceRemaining.tv_sec == 7 && sceRemaining.tv_nsec == 9);

    const KernelTimespec sceSubMicros[] = {
        {0, 1},
        {0, 10},
        {0, 100},
        {0, 500},
        {0, 999},
        {0, 1000},
        {0, 50000}
    };

    for (const auto& request : sceSubMicros) {
        sceRemaining = {13, 17};
        Require(sceKernelNanosleep(&request, &sceRemaining) == SCE_OK);
        Require(sceRemaining.tv_sec == 13 && sceRemaining.tv_nsec == 17);
        Require(sceKernelNanosleep(&request, nullptr) == SCE_OK);
    }
}

static void TestNanosleepNegativeAndValidationEinval() {
    using PosixNanosleep = int (APS5_VABI *)(const KernelTimespec*, KernelTimespec*);
    const PosixNanosleep posixCalls[] = {nanosleep_nid_postfix, _nanosleep_nid_postfix};

    const KernelTimespec invalidRequests[] = {
        {0, -1},
        {0, -500},
        {0, -1000000LL},
        {0, INT64_MIN},
        {-1, -1},
        {-10, -500},
        {0, NANOS_PER_SECOND},
        {0, NANOS_PER_SECOND + 1},
        {0, INT64_MAX},
        {-1, NANOS_PER_SECOND}
    };

    for (auto call : posixCalls) {
        for (const auto& request : invalidRequests) {
            KernelTimespec remaining{5, 8};
            *__error_nid_postfix() = GUEST_EACCES;
            Require(call(&request, &remaining) == -1);
            Require(*__error_nid_postfix() == GUEST_EINVAL);
            Require(remaining.tv_sec == 5 && remaining.tv_nsec == 8);

            Require(call(&request, nullptr) == -1);
            Require(*__error_nid_postfix() == GUEST_EINVAL);
        }

        KernelTimespec remaining{5, 8};
        *__error_nid_postfix() = GUEST_EACCES;
        Require(call(nullptr, &remaining) == -1);
        Require(*__error_nid_postfix() == GUEST_EFAULT);
        Require(remaining.tv_sec == 5 && remaining.tv_nsec == 8);
        Require(call(nullptr, nullptr) == -1);
        Require(*__error_nid_postfix() == GUEST_EFAULT);

        const KernelTimespec negativeSeconds[] = {
            {-1, 0},
            {-1, 500},
            {-1, NANOS_PER_SECOND - 1},
            {-1000, 0},
            {INT64_MIN, 0}
        };

        for (const auto& request : negativeSeconds) {
            remaining = {5, 8};
            *__error_nid_postfix() = GUEST_EACCES;
            Require(call(&request, &remaining) == SCE_OK);
            Require(*__error_nid_postfix() == GUEST_EACCES);
            Require(remaining.tv_sec == 5 && remaining.tv_nsec == 8);
        }
    }

    for (const auto& request : invalidRequests) {
        KernelTimespec remaining{5, 8};
        Require(sceKernelNanosleep(&request, &remaining) == SCE_KERNEL_ERROR_EINVAL);
        Require(remaining.tv_sec == 5 && remaining.tv_nsec == 8);
        Require(sceKernelNanosleep(&request, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    }

    KernelTimespec sceRemaining{5, 8};
    Require(sceKernelNanosleep(nullptr, &sceRemaining) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceRemaining.tv_sec == 5 && sceRemaining.tv_nsec == 8);
    Require(sceKernelNanosleep(nullptr, nullptr) == SCE_KERNEL_ERROR_EFAULT);

    const KernelTimespec sceNegativeSeconds[] = {
        {-1, 0},
        {-1, 500},
        {-1, NANOS_PER_SECOND - 1},
        {-1000, 0},
        {INT64_MIN, 0}
    };

    for (const auto& request : sceNegativeSeconds) {
        sceRemaining = {5, 8};
        Require(sceKernelNanosleep(&request, &sceRemaining) == SCE_OK);
        Require(sceRemaining.tv_sec == 5 && sceRemaining.tv_nsec == 8);
    }
}

static std::atomic<bool> threadStarted{false};
static std::atomic<bool> threadFinished{false};
static std::atomic<int> threadResult{0};

static void* APS5_VABI UsleepWorker(void*) {
    threadStarted.store(true);
    threadResult.store(sceKernelUsleep_nid_postfix(30000));
    threadFinished.store(true);
    return nullptr;
}

static void* APS5_VABI PosixNanosleepWorker(void*) {
    const KernelTimespec request{0, 30000000LL};
    KernelTimespec remaining{11, 22};
    *__error_nid_postfix() = GUEST_EACCES;
    threadStarted.store(true);
    const int res = nanosleep_nid_postfix(&request, &remaining);
    Require(*__error_nid_postfix() == GUEST_EACCES);
    Require(remaining.tv_sec == 11 && remaining.tv_nsec == 22);
    threadResult.store(res);
    threadFinished.store(true);
    return nullptr;
}

static void* APS5_VABI SceNanosleepWorker(void*) {
    const KernelTimespec request{0, 30000000LL};
    KernelTimespec remaining{33, 44};
    threadStarted.store(true);
    const int res = sceKernelNanosleep(&request, &remaining);
    Require(remaining.tv_sec == 33 && remaining.tv_nsec == 44);
    threadResult.store(res);
    threadFinished.store(true);
    return nullptr;
}

static void TestSignalInterruptionSemantics() {
    Require(sceKernelInstallExceptionHandler(0, reinterpret_cast<void*>(&SignalHandler)) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelInstallExceptionHandler(30, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelInstallExceptionHandler(31, reinterpret_cast<void*>(&SignalHandler)) == SCE_KERNEL_ERROR_EINVAL);

    Require(sceKernelInstallExceptionHandler(GUEST_SIGUSR1, reinterpret_cast<void*>(&SignalHandler)) == SCE_OK);
    Require(sceKernelInstallExceptionHandler(GUEST_SIGUSR1, reinterpret_cast<void*>(&SignalHandler)) == SCE_KERNEL_ERROR_EAGAIN);

    Require(sceKernelRaiseException(nullptr, GUEST_SIGUSR1) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelRaiseException(scePthreadSelf(), 1) == SCE_KERNEL_ERROR_EINVAL);

    {
        handlerDelivered.store(0);
        lastSignal.store(0);
        threadStarted.store(false);
        threadFinished.store(false);
        threadResult.store(-1);

        Pthread thread = nullptr;
        Require(scePthreadCreate(&thread, nullptr, UsleepWorker, nullptr, "usleep_signal_worker") == SCE_OK);
        Require(WaitFor([] { return threadStarted.load(); }, std::chrono::milliseconds(3000)));
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        Require(sceKernelRaiseException(thread, GUEST_SIGUSR1) == SCE_OK);
        Require(WaitFor([] { return handlerDelivered.load() == 1; }, std::chrono::milliseconds(3000)));
        Require(lastSignal.load() == GUEST_SIGUSR1);
        Require(WaitFor([] { return threadFinished.load(); }, std::chrono::milliseconds(3000)));
        Require(threadResult.load() == SCE_OK);
        void* joinVal = nullptr;
        Require(scePthreadJoin(thread, &joinVal) == SCE_OK);
    }

    {
        handlerDelivered.store(0);
        lastSignal.store(0);
        threadStarted.store(false);
        threadFinished.store(false);
        threadResult.store(-1);

        Pthread thread = nullptr;
        Require(scePthreadCreate(&thread, nullptr, PosixNanosleepWorker, nullptr, "nanosleep_signal_worker") == SCE_OK);
        Require(WaitFor([] { return threadStarted.load(); }, std::chrono::milliseconds(3000)));
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        Require(sceKernelRaiseException(thread, GUEST_SIGUSR1) == SCE_OK);
        Require(WaitFor([] { return handlerDelivered.load() == 1; }, std::chrono::milliseconds(3000)));
        Require(lastSignal.load() == GUEST_SIGUSR1);
        Require(WaitFor([] { return threadFinished.load(); }, std::chrono::milliseconds(3000)));
        Require(threadResult.load() == SCE_OK);
        void* joinVal = nullptr;
        Require(scePthreadJoin(thread, &joinVal) == SCE_OK);
    }

    {
        handlerDelivered.store(0);
        lastSignal.store(0);
        threadStarted.store(false);
        threadFinished.store(false);
        threadResult.store(-1);

        Pthread thread = nullptr;
        Require(scePthreadCreate(&thread, nullptr, SceNanosleepWorker, nullptr, "sce_nanosleep_signal_worker") == SCE_OK);
        Require(WaitFor([] { return threadStarted.load(); }, std::chrono::milliseconds(3000)));
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        Require(sceKernelRaiseException(thread, GUEST_SIGUSR1) == SCE_OK);
        Require(WaitFor([] { return handlerDelivered.load() == 1; }, std::chrono::milliseconds(3000)));
        Require(lastSignal.load() == GUEST_SIGUSR1);
        Require(WaitFor([] { return threadFinished.load(); }, std::chrono::milliseconds(3000)));
        Require(threadResult.load() == SCE_OK);
        void* joinVal = nullptr;
        Require(scePthreadJoin(thread, &joinVal) == SCE_OK);
    }

    Require(sceKernelRemoveExceptionHandler(0) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelRemoveExceptionHandler(31) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelRemoveExceptionHandler(GUEST_SIGUSR1) == SCE_OK);
}

int main() {
    TestUsleepSubMicrosecondBounds();
    TestNanosleepSubBounds();
    TestNanosleepNegativeAndValidationEinval();
    TestSignalInterruptionSemantics();
    return 0;
}
