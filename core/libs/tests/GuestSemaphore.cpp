#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Semaphore/include/Semaphore.hpp"
#include "SceTypes.hpp"
#include <chrono>
#include <cstdint>
#include <cstdlib>

static constexpr int SCE_OK = 0;

static void Require(bool value) { if (!value) std::abort(); }

struct Waiter {
    KernelSema sem = nullptr;
    bool timed = false;
    int result = SCE_OK;
};

static void* APS5_VABI Wait(void* arg) {
    auto& waiter = *static_cast<Waiter*>(arg);
    KernelUseconds timeout = 5000000;
    waiter.result = sceKernelWaitSema(waiter.sem, 1, waiter.timed ? &timeout : nullptr);
    return nullptr;
}

static void RequireInvalidArguments() {
    KernelSema rejected = nullptr;
    Require(sceKernelCreateSema(&rejected, "rejected", 0, 5, 3, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelCreateSema(&rejected, "rejected", 0, 0, 0, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelCreateSema(&rejected, "rejected", 0, -1, 3, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelCreateSema(&rejected, nullptr, 0, 0, 1, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelCreateSema(&rejected, "rejected", 0, 0, -1, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    for (std::uint32_t attr = 3; attr <= 0x320; ++attr) {
        if (attr >= 0x100 && attr <= 0x102) continue;
        Require(sceKernelCreateSema(&rejected, "rejected", attr, 0, 1, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    }
    for (const std::uint32_t attr : {0x1000u, 0x10000u, 0x80000000u, 0xffffffffu})
        Require(sceKernelCreateSema(&rejected, "rejected", attr, 0, 1, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(rejected == nullptr);

    KernelSema sem = nullptr;
    Require(sceKernelCreateSema(&sem, "limits", 0, 2, 3, nullptr) == SCE_OK);
    Require(sceKernelPollSema(sem, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelPollSema(sem, -1) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelPollSema(sem, 4) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelPollSema(sem, 100) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelPollSema(sem, 3) == SCE_KERNEL_ERROR_EBUSY);
    Require(sceKernelSignalSema(sem, 0) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelSignalSema(sem, -1) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelPollSema(sem, 2) == SCE_OK);
    Require(sceKernelSignalSema(sem, 3) == SCE_OK);
    Require(sceKernelPollSema(sem, 3) == SCE_OK);
    Require(sceKernelDeleteSema(sem) == SCE_OK);
}

int main() {
    KernelUseconds nullTimeout = 1000;
    int nullWaiters = -1;
    Require(sceKernelWaitSema(nullptr, 1, nullptr) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelWaitSema(nullptr, 1, &nullTimeout) == SCE_KERNEL_ERROR_ESRCH && nullTimeout == 1000);
    Require(sceKernelPollSema(nullptr, 1) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelSignalSema(nullptr, 1) == SCE_KERNEL_ERROR_ESRCH);
    Require(sceKernelCancelSema(nullptr, 0, &nullWaiters) == SCE_KERNEL_ERROR_ESRCH && nullWaiters == -1);
    Require(sceKernelDeleteSema(nullptr) == SCE_KERNEL_ERROR_ESRCH);
    KernelSema sem = nullptr;
    Require(sceKernelCreateSema(&sem, "cancel", 0, 1, 3, nullptr) == SCE_OK);

    int waiters = -1;
    Require(sceKernelCancelSema(sem, 4, &waiters) == SCE_KERNEL_ERROR_EINVAL);
    Require(waiters == -1);
    Require(sceKernelPollSema(sem, 1) == SCE_OK);
    Require(sceKernelPollSema(sem, 1) == SCE_KERNEL_ERROR_EBUSY);

    Require(sceKernelCancelSema(sem, 2, &waiters) == SCE_OK && waiters == 0);
    Require(sceKernelPollSema(sem, 2) == SCE_OK);
    Require(sceKernelPollSema(sem, 1) == SCE_KERNEL_ERROR_EBUSY);

    Require(sceKernelCancelSema(sem, -1, nullptr) == SCE_OK);
    Require(sceKernelPollSema(sem, 1) == SCE_OK);
    Require(sceKernelPollSema(sem, 1) == SCE_KERNEL_ERROR_EBUSY);

    Waiter blocked{sem, false};
    Waiter timed{sem, true};
    Pthread blockedThread = nullptr;
    Pthread timedThread = nullptr;
    Require(scePthreadCreate(&blockedThread, nullptr, Wait, &blocked, nullptr) == SCE_OK);
    Require(scePthreadCreate(&timedThread, nullptr, Wait, &timed, nullptr) == SCE_OK);

    int canceled = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    while (canceled < 2 && std::chrono::steady_clock::now() < deadline) {
        waiters = -1;
        Require(sceKernelCancelSema(sem, 0, &waiters) == SCE_OK);
        Require(waiters >= 0);
        canceled += waiters;
    }
    Require(scePthreadJoin(blockedThread, nullptr) == SCE_OK);
    Require(scePthreadJoin(timedThread, nullptr) == SCE_OK);
    Require(canceled == 2);
    Require(blocked.result == SCE_KERNEL_ERROR_ECANCELED);
    Require(timed.result == SCE_KERNEL_ERROR_ECANCELED);
    Require(sceKernelCancelSema(sem, 0, &waiters) == SCE_OK && waiters == 0);
    Require(sceKernelPollSema(sem, 1) == SCE_KERNEL_ERROR_EBUSY);

    KernelUseconds remaining = 1000;
    Require(sceKernelWaitSema(sem, 1, &remaining) == SCE_KERNEL_ERROR_ETIMEDOUT && remaining == 0);
    Require(sceKernelSignalSema(sem, 1) == SCE_OK);
    remaining = 5000000;
    Require(sceKernelWaitSema(sem, 1, &remaining) == SCE_OK && remaining > 0 && remaining <= 5000000);

    Require(sceKernelSignalSema(sem, 1) == SCE_OK);
    Require(sceKernelWaitSema(sem, 1, nullptr) == SCE_OK);
    Require(sceKernelDeleteSema(sem) == SCE_OK);

    RequireInvalidArguments();
}
