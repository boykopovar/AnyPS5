#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <thread>
#include <vector>

extern "C" {

int APS5_Test_GetEventFlagWaiterCount(KernelEventFlag handle);
}

static constexpr int SCE_OK = 0;
static constexpr std::uint32_t EVF_ATTR_MULTI = 0x20;
static constexpr std::uint32_t EVF_WAITMODE_AND = 0x01;
static constexpr std::uint32_t EVF_WAITMODE_OR = 0x02;
static constexpr std::uint32_t EVF_WAITMODE_CLEAR_ALL = 0x10;
static constexpr std::uint32_t EVF_WAITMODE_CLEAR_PAT = 0x20;
static constexpr int WAITERS = 8;

static void Require(bool value) { if (!value) std::abort(); }

static void WaitForWaiters(KernelEventFlag flag, int expected) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (APS5_Test_GetEventFlagWaiterCount(flag) != expected) {
        Require(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
    }
}

struct Waiter {
    std::thread thread;
    std::atomic<bool> done{false};
    int result = -1;
    std::uint64_t pattern = 0;
};

static void StartWaiter(Waiter& waiter, KernelEventFlag flag, std::uint64_t bits, std::uint32_t mode) {
    waiter.thread = std::thread([&waiter, flag, bits, mode] {
        waiter.result = sceKernelWaitEventFlag(flag, bits, mode, &waiter.pattern, nullptr);
        waiter.done = true;
    });
}

static void RequireEachSetReleasesOnlyItsWaiter() {
    KernelEventFlag flag = nullptr;
    Require(sceKernelCreateEventFlag(&flag, "one_each", EVF_ATTR_MULTI, 0, nullptr) == SCE_OK);
    std::array<Waiter, WAITERS> waiters;
    for (int i = 0; i < WAITERS; ++i) StartWaiter(waiters[i], flag, 1ull << i, EVF_WAITMODE_OR | EVF_WAITMODE_CLEAR_PAT);
    WaitForWaiters(flag, WAITERS);
    constexpr std::array<int, WAITERS> order{5, 2, 7, 0, 3, 6, 1, 4};
    for (int k = 0; k < WAITERS; ++k) {
        const int index = order[k];
        Require(sceKernelSetEventFlag(flag, 1ull << index) == SCE_OK);
        waiters[index].thread.join();
        Require(waiters[index].result == SCE_OK);
        Require(waiters[index].pattern == 1ull << index);
        WaitForWaiters(flag, WAITERS - k - 1);
        for (int j = k + 1; j < WAITERS; ++j) Require(!waiters[order[j]].done);
    }
    std::uint64_t remaining = UINT64_MAX;
    Require(sceKernelPollEventFlag(flag, 0xff, EVF_WAITMODE_OR, &remaining) == SCE_KERNEL_ERROR_EBUSY);
    Require(remaining == 0);
    Require(sceKernelDeleteEventFlag(flag) == SCE_OK);
}

static void RequireAndWaiterWaitsForEveryBit() {
    KernelEventFlag flag = nullptr;
    Require(sceKernelCreateEventFlag(&flag, "and_bits", EVF_ATTR_MULTI, 0, nullptr) == SCE_OK);
    Waiter both;
    Waiter other;
    StartWaiter(both, flag, 0x3, EVF_WAITMODE_AND | EVF_WAITMODE_CLEAR_ALL);
    StartWaiter(other, flag, 0x10, EVF_WAITMODE_OR | EVF_WAITMODE_CLEAR_PAT);
    WaitForWaiters(flag, 2);
    Require(sceKernelSetEventFlag(flag, 0x1) == SCE_OK);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    Require(!both.done && !other.done);
    Require(APS5_Test_GetEventFlagWaiterCount(flag) == 2);
    Require(sceKernelSetEventFlag(flag, 0x2) == SCE_OK);
    both.thread.join();
    Require(both.result == SCE_OK);
    Require(both.pattern == 0x3);
    WaitForWaiters(flag, 1);
    Require(!other.done);
    Require(sceKernelSetEventFlag(flag, 0x10) == SCE_OK);
    other.thread.join();
    Require(other.result == SCE_OK);
    Require(other.pattern == 0x10);
    Require(sceKernelDeleteEventFlag(flag) == SCE_OK);
}

static void RequireTimeoutLeavesOtherWaiters() {
    KernelEventFlag flag = nullptr;
    Require(sceKernelCreateEventFlag(&flag, "timeout", EVF_ATTR_MULTI, 0, nullptr) == SCE_OK);
    Waiter blocked;
    StartWaiter(blocked, flag, 0x1, EVF_WAITMODE_OR | EVF_WAITMODE_CLEAR_PAT);
    WaitForWaiters(flag, 1);
    int timedResult = -1;
    std::thread timed([&] {
        std::uint64_t pattern = 0;
        KernelUseconds timeout = 2000;
        timedResult = sceKernelWaitEventFlag(flag, 0x2, EVF_WAITMODE_OR, &pattern, &timeout);
    });
    timed.join();
    Require(timedResult == SCE_KERNEL_ERROR_ETIMEDOUT);
    WaitForWaiters(flag, 1);
    Require(!blocked.done);
    Require(sceKernelSetEventFlag(flag, 0x1) == SCE_OK);
    blocked.thread.join();
    Require(blocked.result == SCE_OK);
    Require(sceKernelDeleteEventFlag(flag) == SCE_OK);
}

static void RequireCancelOrDeleteReleasesEveryWaiter(bool cancel) {
    KernelEventFlag flag = nullptr;
    Require(sceKernelCreateEventFlag(&flag, cancel ? "cancel_all" : "delete_all", EVF_ATTR_MULTI, 0, nullptr) == SCE_OK);
    std::array<Waiter, WAITERS> waiters;
    for (int i = 0; i < WAITERS; ++i) StartWaiter(waiters[i], flag, 0x100, EVF_WAITMODE_OR | EVF_WAITMODE_CLEAR_PAT);
    WaitForWaiters(flag, WAITERS);
    if (cancel) {
        int woken = 0;
        Require(sceKernelCancelEventFlag(flag, 0, &woken) == SCE_OK);
        Require(woken == WAITERS);
    } else {
        Require(sceKernelDeleteEventFlag(flag) == SCE_OK);
    }
    for (auto& waiter : waiters) {
        waiter.thread.join();
        Require(waiter.result == (cancel ? SCE_KERNEL_ERROR_ECANCELED : SCE_KERNEL_ERROR_EACCES));
    }
    if (cancel) Require(sceKernelDeleteEventFlag(flag) == SCE_OK);
}

int main() {
    for (int round = 0; round < 20; ++round) {
        RequireEachSetReleasesOnlyItsWaiter();
        RequireAndWaiterWaitsForEveryBit();
        RequireTimeoutLeavesOtherWaiters();
        RequireCancelOrDeleteReleasesEveryWaiter(true);
        RequireCancelOrDeleteReleasesEveryWaiter(false);
    }
    return 0;
}
