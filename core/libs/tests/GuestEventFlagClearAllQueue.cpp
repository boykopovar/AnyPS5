#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <thread>

extern "C" {

int APS5_Test_GetEventFlagWaiterCount(KernelEventFlag handle);
}

static constexpr std::uint32_t EVF_ATTR_FIFO_MULTI = 0x21;
static constexpr std::uint32_t EVF_WAITMODE_OR = 0x02;
static constexpr std::uint32_t EVF_WAITMODE_CLEAR_ALL = 0x10;

struct Waiter {
    std::thread thread;
    std::atomic<bool> done{false};
    int result = -1;
    std::uint64_t pattern = UINT64_MAX;
};

static void StartWaiter(Waiter& waiter, KernelEventFlag flag, std::uint64_t bits, std::uint32_t mode) {
    waiter.thread = std::thread([&waiter, flag, bits, mode] {
        waiter.result = sceKernelWaitEventFlag(flag, bits, mode, &waiter.pattern, nullptr);
        waiter.done = true;
    });
}

static bool WaitForCompletion(Waiter& waiter, std::chrono::steady_clock::time_point deadline) {
    while (!waiter.done) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::yield();
    }
    return true;
}

static int WaitForWaiters(KernelEventFlag flag, int expected) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    int actual = APS5_Test_GetEventFlagWaiterCount(flag);
    while (actual != expected && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
        actual = APS5_Test_GetEventFlagWaiterCount(flag);
    }
    return actual;
}

int main() {
    KernelEventFlag flag = nullptr;
    Waiter first;
    Waiter second;
    const auto fail = [&] {
        if (flag != nullptr) {
            const int result = sceKernelCancelEventFlag(flag, 0, nullptr);
            if (result != 0) {
                std::fprintf(stderr, "cleanup cancel: expected 0, actual 0x%x\n", static_cast<unsigned>(result));
                std::abort();
            }
        }
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        for (Waiter* waiter : {&first, &second}) {
            if (!waiter->thread.joinable()) continue;
            if (!WaitForCompletion(*waiter, deadline)) {
                std::fprintf(stderr, "cleanup completion: expected 1, actual 0\n");
                std::abort();
            }
            waiter->thread.join();
        }
        if (flag != nullptr) {
            const int result = sceKernelDeleteEventFlag(flag);
            if (result != 0) {
                std::fprintf(stderr, "cleanup delete: expected 0, actual 0x%x\n", static_cast<unsigned>(result));
                std::abort();
            }
        }
        std::exit(EXIT_FAILURE);
    };
    const auto require = [&](const char* stage, std::uint64_t actual, std::uint64_t expected) {
        if (actual == expected) return;
        std::fprintf(stderr, "%s: expected 0x%llx, actual 0x%llx\n", stage,
                     static_cast<unsigned long long>(expected), static_cast<unsigned long long>(actual));
        fail();
    };

    require("create", sceKernelCreateEventFlag(&flag, "clear_all_queue", EVF_ATTR_FIFO_MULTI, 0, nullptr), 0);
    StartWaiter(first, flag, 1, EVF_WAITMODE_OR | EVF_WAITMODE_CLEAR_ALL);
    require("first registered", WaitForWaiters(flag, 1), 1);
    StartWaiter(second, flag, 2, EVF_WAITMODE_OR);
    require("both registered", WaitForWaiters(flag, 2), 2);

    require("set 3", sceKernelSetEventFlag(flag, 3), 0);
    require("first completion", WaitForCompletion(first, std::chrono::steady_clock::now() + std::chrono::seconds(2)), true);
    first.thread.join();
    require("first result", first.result, 0);
    require("first pattern", first.pattern, 3);

    std::uint64_t remaining = UINT64_MAX;
    require("poll result", static_cast<std::uint32_t>(sceKernelPollEventFlag(flag, 2, EVF_WAITMODE_OR, &remaining)),
            static_cast<std::uint32_t>(SCE_KERNEL_ERROR_EBUSY));
    require("poll pattern", remaining, 0);
    require("second registered", APS5_Test_GetEventFlagWaiterCount(flag), 1);

    require("set 2", sceKernelSetEventFlag(flag, 2), 0);
    require("second completion", WaitForCompletion(second, std::chrono::steady_clock::now() + std::chrono::seconds(2)), true);
    second.thread.join();
    require("second result", second.result, 0);
    require("second pattern", second.pattern, 2);
    require("delete", sceKernelDeleteEventFlag(flag), 0);
    return 0;
}
