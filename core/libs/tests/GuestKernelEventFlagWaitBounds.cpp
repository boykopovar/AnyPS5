#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <thread>

extern "C" int APS5_Test_GetEventFlagWaiterCount(KernelEventFlag handle);

namespace {

constexpr int SceOk = 0;

constexpr std::uint32_t EvfAttrSingle = 0x10;
constexpr std::uint32_t EvfAttrMulti = 0x20;
constexpr std::uint32_t EvfAttrShared = 0x100;

constexpr std::uint32_t EvfWaitModeAnd = 0x01;
constexpr std::uint32_t EvfWaitModeOr = 0x02;
constexpr std::uint32_t EvfWaitModeClearAll = 0x10;
constexpr std::uint32_t EvfWaitModeClearPat = 0x20;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestWaitInvalidHandle() {
    KernelEventFlag ef = nullptr;
    Require(sceKernelWaitEventFlag(ef, 1, EvfWaitModeAnd, nullptr, nullptr) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelWaitEventFlag(reinterpret_cast<KernelEventFlag>(0x1234), 1, EvfWaitModeAnd, nullptr, nullptr) == SCE_KERNEL_ERROR_ESRCH);

    Require(sceKernelCreateEventFlag(&ef, "evf_del_hdl", EvfAttrMulti, 0, nullptr) == SceOk);
    Require(ef != nullptr);
    Require(sceKernelDeleteEventFlag(ef) == SceOk);
    Require(sceKernelWaitEventFlag(ef, 1, EvfWaitModeAnd, nullptr, nullptr) == SCE_KERNEL_ERROR_ESRCH);
}

void TestWaitInvalidParameters() {
    KernelEventFlag ef = nullptr;
    Require(sceKernelCreateEventFlag(&ef, "evf_invalid_params", EvfAttrMulti, 0x55, nullptr) == SceOk);
    Require(ef != nullptr);

    Require(sceKernelWaitEventFlag(ef, 0, EvfWaitModeAnd, nullptr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelWaitEventFlag(ef, 0, EvfWaitModeOr, nullptr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelWaitEventFlag(ef, 0, EvfWaitModeAnd | EvfWaitModeClearAll, nullptr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelWaitEventFlag(ef, 0, EvfWaitModeOr | EvfWaitModeClearPat, nullptr, nullptr) == SCE_KERNEL_ERROR_EINVAL);

    const std::uint32_t invalidModes[] = {
        0u,
        EvfWaitModeAnd | EvfWaitModeOr,
        EvfWaitModeClearAll,
        EvfWaitModeClearPat,
        EvfWaitModeClearAll | EvfWaitModeClearPat,
        EvfWaitModeAnd | EvfWaitModeClearAll | EvfWaitModeClearPat,
        EvfWaitModeOr | EvfWaitModeClearAll | EvfWaitModeClearPat,
        0x04u,
        0x08u,
        0x40u,
        0x80u,
        0x100u,
        0x80000000u
    };
    for (const auto mode : invalidModes) {
        Require(sceKernelWaitEventFlag(ef, 1, mode, nullptr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    }

    Require(sceKernelDeleteEventFlag(ef) == SceOk);
}

void TestWaitImmediateSatisfactionModes() {
    KernelEventFlag ef = nullptr;
    Require(sceKernelCreateEventFlag(&ef, "evf_modes", EvfAttrMulti, 0x55, nullptr) == SceOk);
    Require(ef != nullptr);

    std::uint64_t result = 0;
    Require(sceKernelWaitEventFlag(ef, 0x01, EvfWaitModeAnd, &result, nullptr) == SceOk);
    Require(result == 0x55);
    Require(sceKernelWaitEventFlag(ef, 0x05, EvfWaitModeAnd, &result, nullptr) == SceOk);
    Require(result == 0x55);
    Require(sceKernelWaitEventFlag(ef, 0x15, EvfWaitModeAnd, nullptr, nullptr) == SceOk);

    Require(sceKernelWaitEventFlag(ef, 0x02 | 0x04, EvfWaitModeOr, &result, nullptr) == SceOk);
    Require(result == 0x55);
    Require(sceKernelWaitEventFlag(ef, 0x80 | 0x40, EvfWaitModeOr, nullptr, nullptr) == SceOk);

    Require(sceKernelWaitEventFlag(ef, 0x01, EvfWaitModeAnd | EvfWaitModeClearPat, &result, nullptr) == SceOk);
    Require(result == 0x55);

    KernelUseconds timeout = 0;
    Require(sceKernelWaitEventFlag(ef, 0x01, EvfWaitModeAnd, &result, &timeout) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(result == 0x54);

    Require(sceKernelWaitEventFlag(ef, 0x04, EvfWaitModeAnd | EvfWaitModeClearPat, &result, nullptr) == SceOk);
    Require(result == 0x54);

    timeout = 0;
    Require(sceKernelWaitEventFlag(ef, 0x04, EvfWaitModeAnd, &result, &timeout) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(result == 0x50);

    Require(sceKernelWaitEventFlag(ef, 0x10, EvfWaitModeAnd | EvfWaitModeClearAll, &result, nullptr) == SceOk);
    Require(result == 0x50);

    timeout = 0;
    Require(sceKernelWaitEventFlag(ef, 0x40, EvfWaitModeOr, &result, &timeout) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(result == 0);

    Require(sceKernelDeleteEventFlag(ef) == SceOk);
}

void TestWaitTimeoutBounds() {
    KernelEventFlag ef = nullptr;
    Require(sceKernelCreateEventFlag(&ef, "evf_timeout_test", EvfAttrMulti, 0x0A, nullptr) == SceOk);
    Require(ef != nullptr);

    KernelUseconds timeout = 0;
    std::uint64_t result = 0;
    Require(sceKernelWaitEventFlag(ef, 0x01, EvfWaitModeAnd, &result, &timeout) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(result == 0x0A);
    Require(timeout == 0);

    timeout = 5000;
    result = 0;
    const auto start = std::chrono::steady_clock::now();
    Require(sceKernelWaitEventFlag(ef, 0x01, EvfWaitModeAnd, &result, &timeout) == SCE_KERNEL_ERROR_ETIMEDOUT);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    Require(elapsed >= std::chrono::milliseconds(1));
    Require(result == 0x0A);
    Require(timeout == 0);

    std::atomic<int> waitStatus{-1};
    std::atomic<std::uint64_t> observedResult{0};
    std::atomic<KernelUseconds> remainingTimeout{0};

    std::thread waiter([&] {
        KernelUseconds to = 500000;
        std::uint64_t res = 0;
        const int status = sceKernelWaitEventFlag(ef, 0x40, EvfWaitModeAnd, &res, &to);
        observedResult.store(res);
        remainingTimeout.store(to);
        waitStatus.store(status);
    });

    while (APS5_Test_GetEventFlagWaiterCount(ef) == 0) {
        std::this_thread::yield();
    }

    Require(sceKernelSetEventFlag(ef, 0x40) == SceOk);
    waiter.join();
    Require(waitStatus.load() == SceOk);
    Require(observedResult.load() == 0x4A);
    Require(remainingTimeout.load() > 0 && remainingTimeout.load() <= 500000);

    Require(sceKernelDeleteEventFlag(ef) == SceOk);
}

void TestWaitSingleWaiterExclusivity() {
    KernelEventFlag ef = nullptr;
    Require(sceKernelCreateEventFlag(&ef, "evf_single_wait", EvfAttrSingle, 0, nullptr) == SceOk);
    Require(ef != nullptr);

    std::atomic<int> waitStatus{-1};
    std::thread waiter([&] {
        KernelUseconds to = 500000;
        waitStatus.store(sceKernelWaitEventFlag(ef, 0x100, EvfWaitModeAnd, nullptr, &to));
    });

    while (APS5_Test_GetEventFlagWaiterCount(ef) == 0) {
        std::this_thread::yield();
    }

    Require(sceKernelPollEventFlag(ef, 0x1, EvfWaitModeAnd, nullptr) == SCE_KERNEL_ERROR_EPERM);
    std::uint64_t rejectedResult = 0xAA;
    Require(sceKernelWaitEventFlag(ef, 0x1, EvfWaitModeAnd, &rejectedResult, nullptr) == SCE_KERNEL_ERROR_EPERM);
    Require(rejectedResult == 0);

    Require(sceKernelSetEventFlag(ef, 0x100) == SceOk);
    waiter.join();
    Require(waitStatus.load() == SceOk);

    Require(sceKernelWaitEventFlag(ef, 0x100, EvfWaitModeAnd, nullptr, nullptr) == SceOk);
    Require(sceKernelDeleteEventFlag(ef) == SceOk);
}

void TestWaitMultiWaiterWakeup() {
    KernelEventFlag ef = nullptr;
    Require(sceKernelCreateEventFlag(&ef, "evf_multi_wait", EvfAttrMulti, 0, nullptr) == SceOk);
    Require(ef != nullptr);

    std::atomic<int> status1{-1};
    std::atomic<int> status2{-1};
    std::atomic<int> status3{-1};

    std::thread t1([&] { status1.store(sceKernelWaitEventFlag(ef, 0x01, EvfWaitModeAnd, nullptr, nullptr)); });
    std::thread t2([&] { status2.store(sceKernelWaitEventFlag(ef, 0x02, EvfWaitModeAnd, nullptr, nullptr)); });
    std::thread t3([&] { status3.store(sceKernelWaitEventFlag(ef, 0x01 | 0x02, EvfWaitModeAnd, nullptr, nullptr)); });

    while (APS5_Test_GetEventFlagWaiterCount(ef) < 3) {
        std::this_thread::yield();
    }

    Require(sceKernelSetEventFlag(ef, 0x01) == SceOk);
    t1.join();
    Require(status1.load() == SceOk);
    Require(status2.load() == -1);
    Require(status3.load() == -1);
    Require(APS5_Test_GetEventFlagWaiterCount(ef) == 2);

    Require(sceKernelSetEventFlag(ef, 0x02) == SceOk);
    t2.join();
    t3.join();
    Require(status2.load() == SceOk);
    Require(status3.load() == SceOk);
    Require(APS5_Test_GetEventFlagWaiterCount(ef) == 0);

    Require(sceKernelDeleteEventFlag(ef) == SceOk);
}

void TestWaitCancellationBounds() {
    KernelEventFlag ef = nullptr;
    Require(sceKernelCreateEventFlag(&ef, "evf_cancel_wait", EvfAttrMulti, 0, nullptr) == SceOk);
    Require(ef != nullptr);

    std::atomic<int> waitStatus{0};
    std::uint64_t resultPat = 0xBeef;

    std::thread waiter([&] {
        KernelUseconds to = 500000;
        waitStatus.store(sceKernelWaitEventFlag(ef, 0x200, EvfWaitModeAnd, &resultPat, &to));
    });

    while (APS5_Test_GetEventFlagWaiterCount(ef) == 0) {
        std::this_thread::yield();
    }

    int numWaiting = -1;
    Require(sceKernelCancelEventFlag(ef, 0x42, &numWaiting) == SceOk);
    Require(numWaiting == 1);

    waiter.join();
    Require(waitStatus.load() == SCE_KERNEL_ERROR_ECANCELED);
    Require(resultPat == 0);

    std::uint64_t currentPat = 0;
    Require(sceKernelPollEventFlag(ef, 0x42, EvfWaitModeAnd, &currentPat) == SceOk);
    Require(currentPat == 0x42);

    Require(sceKernelDeleteEventFlag(ef) == SceOk);
}

void TestWaitDeletionBounds() {
    KernelEventFlag ef = nullptr;
    Require(sceKernelCreateEventFlag(&ef, "evf_delete_wait", EvfAttrMulti, 0, nullptr) == SceOk);
    Require(ef != nullptr);

    std::atomic<int> waitStatus{0};
    std::uint64_t resultPat = 0xBeef;

    std::thread waiter([&] {
        KernelUseconds to = 500000;
        waitStatus.store(sceKernelWaitEventFlag(ef, 0x800, EvfWaitModeAnd, &resultPat, &to));
    });

    while (APS5_Test_GetEventFlagWaiterCount(ef) == 0) {
        std::this_thread::yield();
    }

    Require(sceKernelDeleteEventFlag(ef) == SceOk);

    waiter.join();
    Require(waitStatus.load() == SCE_KERNEL_ERROR_EACCES);
    Require(resultPat == 0);

    Require(sceKernelWaitEventFlag(ef, 0x800, EvfWaitModeAnd, nullptr, nullptr) == SCE_KERNEL_ERROR_ESRCH);
}

void TestWaitSharedOpenedBounds() {
    KernelEventFlag creator = nullptr;
    Require(sceKernelCreateEventFlag(&creator, "evf_shared_wait_test", EvfAttrShared | EvfAttrMulti, 0, nullptr) == SceOk);
    Require(creator != nullptr);

    KernelEventFlag opened = nullptr;
    Require(sceKernelOpenEventFlag(&opened, "evf_shared_wait_test") == SceOk);
    Require(opened != nullptr);

    std::atomic<int> waitStatus{-1};
    std::uint64_t result = 0;
    std::thread waiter([&] {
        waitStatus.store(sceKernelWaitEventFlag(opened, 0x55, EvfWaitModeAnd, &result, nullptr));
    });

    while (APS5_Test_GetEventFlagWaiterCount(opened) == 0) {
        std::this_thread::yield();
    }

    Require(sceKernelSetEventFlag(creator, 0x55) == SceOk);
    waiter.join();
    Require(waitStatus.load() == SceOk);
    Require(result == 0x55);

    Require(sceKernelCloseEventFlag(opened) == SceOk);
    Require(sceKernelWaitEventFlag(opened, 0x55, EvfWaitModeAnd, nullptr, nullptr) == SCE_KERNEL_ERROR_ESRCH);

    Require(sceKernelDeleteEventFlag(creator) == SceOk);
}

void TestWait64BitPatternBounds() {
    KernelEventFlag ef = nullptr;
    Require(sceKernelCreateEventFlag(&ef, "evf_64bit_wait", EvfAttrMulti, 0x8000000000000001ULL, nullptr) == SceOk);
    Require(ef != nullptr);

    std::uint64_t result = 0;
    Require(sceKernelWaitEventFlag(ef, 0x8000000000000000ULL, EvfWaitModeAnd, &result, nullptr) == SceOk);
    Require(result == 0x8000000000000001ULL);

    Require(sceKernelWaitEventFlag(ef, 0x8000000000000000ULL, EvfWaitModeAnd | EvfWaitModeClearPat, &result, nullptr) == SceOk);
    Require(result == 0x8000000000000001ULL);

    KernelUseconds timeout = 0;
    Require(sceKernelWaitEventFlag(ef, 0x8000000000000000ULL, EvfWaitModeAnd, &result, &timeout) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(result == 0x0000000000000001ULL);

    Require(sceKernelWaitEventFlag(ef, 0x0000000000000001ULL, EvfWaitModeAnd | EvfWaitModeClearAll, &result, nullptr) == SceOk);
    Require(result == 0x0000000000000001ULL);

    timeout = 0;
    Require(sceKernelWaitEventFlag(ef, 0x0000000000000001ULL, EvfWaitModeAnd, &result, &timeout) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(result == 0);

    Require(sceKernelDeleteEventFlag(ef) == SceOk);
}

} // namespace

int main() {
    TestWaitInvalidHandle();
    TestWaitInvalidParameters();
    TestWaitImmediateSatisfactionModes();
    TestWaitTimeoutBounds();
    TestWaitSingleWaiterExclusivity();
    TestWaitMultiWaiterWakeup();
    TestWaitCancellationBounds();
    TestWaitDeletionBounds();
    TestWaitSharedOpenedBounds();
    TestWait64BitPatternBounds();
    return 0;
}
