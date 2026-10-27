#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <thread>

extern "C" {

int APS5_Test_GetEventFlagWaiterCount(KernelEventFlag handle);
}

static constexpr int SCE_OK = 0;
static constexpr std::uint32_t EVF_ATTR_MULTI = 0x20;
static constexpr std::uint32_t EVF_WAITMODE_OR = 0x02;
static constexpr std::uint32_t EVF_WAITMODE_CLEAR_PAT = 0x20;
static constexpr int ROUNDS = 200;

static void Require(bool value) { if (!value) std::abort(); }

static void WaitForWaiters(KernelEventFlag flag, int expected) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (APS5_Test_GetEventFlagWaiterCount(flag) != expected) {
        Require(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
    }
}

static void RequireReleasedWaiterKeepsResult(bool cancel) {
    int consumed = 0;
    for (int round = 0; round < ROUNDS; ++round) {
        KernelEventFlag flag = nullptr;
        Require(sceKernelCreateEventFlag(&flag, "released", EVF_ATTR_MULTI, 0, nullptr) == SCE_OK);
        int result = -1;
        std::uint64_t pattern = 0;
        std::thread waiter([&] { result = sceKernelWaitEventFlag(flag, 1, EVF_WAITMODE_OR | EVF_WAITMODE_CLEAR_PAT, &pattern, nullptr); });
        WaitForWaiters(flag, 1);
        Require(sceKernelSetEventFlag(flag, 1) == SCE_OK);
        std::uint64_t remaining = 0;
        Require(sceKernelPollEventFlag(flag, 0x80, EVF_WAITMODE_OR, &remaining) == SCE_KERNEL_ERROR_EBUSY);
        if ((remaining & 1) == 0) ++consumed;
        if (cancel) {
            int woken = 0;
            Require(sceKernelCancelEventFlag(flag, 0, &woken) == SCE_OK);
        } else {
            Require(sceKernelDeleteEventFlag(flag) == SCE_OK);
        }
        waiter.join();
        if ((remaining & 1) == 0) {
            Require(result == SCE_OK);
            Require(pattern == 1);
        }
        if (cancel) Require(sceKernelDeleteEventFlag(flag) == SCE_OK);
    }
    Require(consumed == ROUNDS);
}

static void RequireCancelWinsBeforeSet() {
    for (int round = 0; round < ROUNDS; ++round) {
        KernelEventFlag flag = nullptr;
        Require(sceKernelCreateEventFlag(&flag, "cancel_first", EVF_ATTR_MULTI, 0, nullptr) == SCE_OK);
        int result = -1;
        std::uint64_t pattern = UINT64_MAX;
        std::thread waiter([&] { result = sceKernelWaitEventFlag(flag, 1, EVF_WAITMODE_OR | EVF_WAITMODE_CLEAR_PAT, &pattern, nullptr); });
        WaitForWaiters(flag, 1);
        int woken = 0;
        Require(sceKernelCancelEventFlag(flag, 0, &woken) == SCE_OK);
        Require(woken == 1);
        Require(sceKernelSetEventFlag(flag, 1) == SCE_OK);
        waiter.join();
        Require(result == SCE_KERNEL_ERROR_ECANCELED);
        Require(pattern == 0);
        std::uint64_t remaining = 0;
        Require(sceKernelPollEventFlag(flag, 1, EVF_WAITMODE_OR, &remaining) == SCE_OK);
        Require(remaining == 1);
        Require(sceKernelDeleteEventFlag(flag) == SCE_OK);
    }
}

static void RequireDeleteWinsBeforeSet() {
    for (int round = 0; round < ROUNDS; ++round) {
        KernelEventFlag flag = nullptr;
        Require(sceKernelCreateEventFlag(&flag, "delete_first", EVF_ATTR_MULTI, 0, nullptr) == SCE_OK);
        int result = -1;
        std::uint64_t pattern = UINT64_MAX;
        std::thread waiter([&] { result = sceKernelWaitEventFlag(flag, 1, EVF_WAITMODE_OR | EVF_WAITMODE_CLEAR_PAT, &pattern, nullptr); });
        WaitForWaiters(flag, 1);
        Require(sceKernelDeleteEventFlag(flag) == SCE_OK);
        waiter.join();
        Require(result == SCE_KERNEL_ERROR_EACCES);
        Require(pattern == 0);
    }
}

static void RequireTimeoutWinsBeforeSet() {
    for (int round = 0; round < ROUNDS; ++round) {
        KernelEventFlag flag = nullptr;
        Require(sceKernelCreateEventFlag(&flag, "timeout_first", EVF_ATTR_MULTI, 0, nullptr) == SCE_OK);
        int result = -1;
        std::uint64_t pattern = UINT64_MAX;
        KernelUseconds timeout = 1000;
        std::thread waiter([&] { result = sceKernelWaitEventFlag(flag, 1, EVF_WAITMODE_OR | EVF_WAITMODE_CLEAR_PAT, &pattern, &timeout); });
        waiter.join();
        Require(result == SCE_KERNEL_ERROR_ETIMEDOUT);
        Require(pattern == 0);
        Require(sceKernelSetEventFlag(flag, 1) == SCE_OK);
        std::uint64_t remaining = 0;
        Require(sceKernelPollEventFlag(flag, 1, EVF_WAITMODE_OR, &remaining) == SCE_OK);
        Require(remaining == 1);
        Require(sceKernelDeleteEventFlag(flag) == SCE_OK);
    }
}

int main() {
    RequireReleasedWaiterKeepsResult(true);
    RequireReleasedWaiterKeepsResult(false);
    RequireCancelWinsBeforeSet();
    RequireDeleteWinsBeforeSet();
    RequireTimeoutWinsBeforeSet();
    return 0;
}
