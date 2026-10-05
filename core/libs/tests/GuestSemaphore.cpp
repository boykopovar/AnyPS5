#include "SceTypes.hpp"
#include <chrono>
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI sceKernelCreateSema(KernelSema* sem, const char* name, uint32_t attr, int init, int max, void* opt);
int APS5_VABI sceKernelDeleteSema(KernelSema sem);
int APS5_VABI sceKernelPollSema(KernelSema sem, int need);
int APS5_VABI sceKernelSignalSema(KernelSema sem, int count);
int APS5_VABI sceKernelWaitSema(KernelSema sem, int need, KernelUseconds* time);
int APS5_VABI sceKernelCancelSema(KernelSema sem, int count, int* threads);
}

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EBUSY = static_cast<int>(0x80020010);
static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);
static constexpr int SCE_KERNEL_ERROR_ECANCELED = static_cast<int>(0x80020055);

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

int main() {
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

    Require(sceKernelSignalSema(sem, 1) == SCE_OK);
    Require(sceKernelWaitSema(sem, 1, nullptr) == SCE_OK);
    Require(sceKernelDeleteSema(sem) == SCE_OK);
}
