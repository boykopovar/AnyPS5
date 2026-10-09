#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <thread>

extern "C" {
int APS5_VABI sceKernelInstallExceptionHandler(int signum, void* handler);
int APS5_VABI sceKernelRemoveExceptionHandler(int signum);
int APS5_VABI sceKernelRaiseException(Pthread thread, int signum);
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
Pthread APS5_VABI scePthreadSelf();
int APS5_VABI sceKernelCreateSema(KernelSema* sem, const char* name, uint32_t attr, int init, int max, void* opt);
int APS5_VABI sceKernelDeleteSema(KernelSema sem);
int APS5_VABI sceKernelSignalSema(KernelSema sem, int count);
int APS5_VABI sceKernelWaitSema(KernelSema sem, int need, KernelUseconds* time);
int APS5_VABI sceKernelSyncOnAddressWait(std::uint32_t* address, std::uint32_t expected, const KernelUseconds* timeout, const char* name);
}

static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);
static constexpr int SCE_KERNEL_ERROR_ESRCH = static_cast<int>(0x80020003);
static constexpr int SCE_KERNEL_ERROR_ETIMEDOUT = static_cast<int>(0x8002003c);
static constexpr int SIGUSR1 = 30;
static constexpr int Repeats = 100;

static void Require(bool value) { if (!value) std::abort(); }

static std::atomic<int> calls{0};
static std::atomic<std::thread::id> handlerThread;
static std::atomic<std::uint64_t> handlerRsp{0};
static std::atomic<std::uintptr_t> handlerFrame{0};
static std::atomic<bool> clobberVectors{false};

static void APS5_VABI Handler(int signum, void* context) {
    Require(signum == SIGUSR1);
    std::uint64_t rsp = 0;
    std::memcpy(&rsp, static_cast<unsigned char*>(context) + 0xf8, sizeof(rsp));
    int local = 0;
    handlerFrame.store(reinterpret_cast<std::uintptr_t>(&local));
    handlerRsp.store(rsp);
    handlerThread.store(std::this_thread::get_id());
    if (clobberVectors.load()) asm volatile("vzeroall" ::: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15");
    calls.fetch_add(1);
}

struct Worker {
    std::atomic<bool> started{false};
    std::atomic<bool> stop{false};
    std::thread::id id;
    KernelSema sem = nullptr;
    int waitResult = -1;
};

static void* APS5_VABI Busy(void* arg) {
    auto& worker = *static_cast<Worker*>(arg);
    worker.id = std::this_thread::get_id();
    worker.started.store(true);
    volatile std::uint64_t spins = 0;
    while (!worker.stop.load()) spins = spins + 1;
    return nullptr;
}

static void* APS5_VABI Waiting(void* arg) {
    auto& worker = *static_cast<Worker*>(arg);
    worker.id = std::this_thread::get_id();
    worker.started.store(true);
    worker.waitResult = sceKernelWaitSema(worker.sem, 1, nullptr);
    return nullptr;
}

static std::mutex hostLock;

static constexpr int HostRounds = 20;
static std::atomic<int> hostRound{0};
static std::atomic<int> hostAcquired{0};

static void* APS5_VABI HostBlocked(void* arg) {
    auto& worker = *static_cast<Worker*>(arg);
    worker.id = std::this_thread::get_id();
    for (int round = 0; round < HostRounds; ++round) {
        while (hostRound.load() != round) std::this_thread::yield();
        worker.started.store(true);
        std::lock_guard lock(hostLock);
        hostAcquired.store(round + 1);
    }
    return nullptr;
}

static constexpr int LeavingRounds = 200;
static std::atomic<int> leavingRound{0};

static void* APS5_VABI Leaving(void* arg) {
    auto& worker = *static_cast<Worker*>(arg);
    worker.id = std::this_thread::get_id();
    for (int round = 0; round < LeavingRounds; ++round) {
        worker.started.store(true);
        Require(sceKernelWaitSema(worker.sem, 1, nullptr) == 0);
        volatile std::uint64_t spins = 0;
        while (leavingRound.load() == round) spins = spins + 1;
    }
    return nullptr;
}

static constexpr int VectorRounds = 100;
static constexpr std::size_t VectorBytes = 16 * 32;
alignas(32) static std::uint8_t vectorPattern[VectorBytes];
alignas(32) static std::uint8_t vectorResult[VectorBytes];
static volatile std::uint8_t vectorStop = 0;

static void* APS5_VABI Vectors(void* arg) {
    auto& worker = *static_cast<Worker*>(arg);
    worker.id = std::this_thread::get_id();
    worker.started.store(true);
    asm volatile(
        "vmovdqu 0(%[pattern]), %%ymm0\n"
        "vmovdqu 32(%[pattern]), %%ymm1\n"
        "vmovdqu 64(%[pattern]), %%ymm2\n"
        "vmovdqu 96(%[pattern]), %%ymm3\n"
        "vmovdqu 128(%[pattern]), %%ymm4\n"
        "vmovdqu 160(%[pattern]), %%ymm5\n"
        "vmovdqu 192(%[pattern]), %%ymm6\n"
        "vmovdqu 224(%[pattern]), %%ymm7\n"
        "vmovdqu 256(%[pattern]), %%ymm8\n"
        "vmovdqu 288(%[pattern]), %%ymm9\n"
        "vmovdqu 320(%[pattern]), %%ymm10\n"
        "vmovdqu 352(%[pattern]), %%ymm11\n"
        "vmovdqu 384(%[pattern]), %%ymm12\n"
        "vmovdqu 416(%[pattern]), %%ymm13\n"
        "vmovdqu 448(%[pattern]), %%ymm14\n"
        "vmovdqu 480(%[pattern]), %%ymm15\n"
        "1:\n"
        "pause\n"
        "cmpb $0, (%[stop])\n"
        "je 1b\n"
        "vmovdqu %%ymm0, 0(%[result])\n"
        "vmovdqu %%ymm1, 32(%[result])\n"
        "vmovdqu %%ymm2, 64(%[result])\n"
        "vmovdqu %%ymm3, 96(%[result])\n"
        "vmovdqu %%ymm4, 128(%[result])\n"
        "vmovdqu %%ymm5, 160(%[result])\n"
        "vmovdqu %%ymm6, 192(%[result])\n"
        "vmovdqu %%ymm7, 224(%[result])\n"
        "vmovdqu %%ymm8, 256(%[result])\n"
        "vmovdqu %%ymm9, 288(%[result])\n"
        "vmovdqu %%ymm10, 320(%[result])\n"
        "vmovdqu %%ymm11, 352(%[result])\n"
        "vmovdqu %%ymm12, 384(%[result])\n"
        "vmovdqu %%ymm13, 416(%[result])\n"
        "vmovdqu %%ymm14, 448(%[result])\n"
        "vmovdqu %%ymm15, 480(%[result])\n"
        "vzeroupper\n"
        :
        : [pattern] "r"(vectorPattern), [result] "r"(vectorResult), [stop] "r"(&vectorStop)
        : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15");
    return nullptr;
}

static std::atomic<bool> finishedReturned{false};

static void* APS5_VABI Finished(void*) {
    finishedReturned.store(true);
    return nullptr;
}

static std::uint32_t nestedWord = 0;
static KernelUseconds nestedTimeout = 0;
static std::atomic<bool> nestedEntered{false};
static std::atomic<int> nestedResult{0};
static std::atomic<int> nestedCalls{0};

static void APS5_VABI NestedHandler(int signum, void*) {
    Require(signum == SIGUSR1);
    nestedEntered.store(true);
    KernelUseconds timeout = nestedTimeout;
    nestedResult.store(sceKernelSyncOnAddressWait(&nestedWord, 0, &timeout, "nested"));
    nestedCalls.fetch_add(1);
}

struct SemaWaiter {
    KernelSema sem = nullptr;
    std::atomic<bool> started{false};
    std::atomic<bool> returned{false};
    int result = -1;
};

static void* APS5_VABI WaitSemaOnce(void* arg) {
    auto& waiter = *static_cast<SemaWaiter*>(arg);
    waiter.started.store(true);
    waiter.result = sceKernelWaitSema(waiter.sem, 1, nullptr);
    waiter.returned.store(true);
    return nullptr;
}

static void StartSemaWaiter(Pthread* thread, SemaWaiter& waiter, KernelSema sem, const char* name) {
    waiter.sem = sem;
    Require(scePthreadCreate(thread, nullptr, WaitSemaOnce, &waiter, name) == 0);
    while (!waiter.started.load()) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

static void ExpectReturned(SemaWaiter& waiter) {
    for (int attempt = 0; attempt < 5000 && !waiter.returned.load(); ++attempt) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Require(waiter.returned.load());
    Require(waiter.result == 0);
}

static void ExpectNestedDone(int before) {
    for (int attempt = 0; attempt < 5000 && nestedCalls.load() == before; ++attempt) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Require(nestedCalls.load() == before + 1);
    Require(nestedResult.load() == SCE_KERNEL_ERROR_ETIMEDOUT);
}

static void NestedWaitKeepsLaterWaiters() {
    KernelSema sem = nullptr;
    Require(sceKernelCreateSema(&sem, "nested later", 0, 0, 2, nullptr) == 0);
    SemaWaiter first;
    SemaWaiter second;
    Pthread firstThread = nullptr;
    Pthread secondThread = nullptr;
    StartSemaWaiter(&firstThread, first, sem, "nested first");
    StartSemaWaiter(&secondThread, second, sem, "nested second");
    nestedTimeout = 20000;
    const int before = nestedCalls.load();
    Require(sceKernelRaiseException(firstThread, SIGUSR1) == 0);
    ExpectNestedDone(before);
    Require(sceKernelSignalSema(sem, 2) == 0);
    ExpectReturned(first);
    ExpectReturned(second);
    Require(scePthreadJoin(firstThread, nullptr) == 0);
    Require(scePthreadJoin(secondThread, nullptr) == 0);
    Require(sceKernelDeleteSema(sem) == 0);
}

static void NestedWaitKeepsOuterWake() {
    KernelSema sem = nullptr;
    Require(sceKernelCreateSema(&sem, "nested outer", 0, 0, 1, nullptr) == 0);
    SemaWaiter waiter;
    Pthread thread = nullptr;
    StartSemaWaiter(&thread, waiter, sem, "nested outer");
    nestedTimeout = 1000000;
    nestedEntered.store(false);
    const int before = nestedCalls.load();
    Require(sceKernelRaiseException(thread, SIGUSR1) == 0);
    while (!nestedEntered.load()) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(sceKernelSignalSema(sem, 1) == 0);
    ExpectNestedDone(before);
    ExpectReturned(waiter);
    Require(scePthreadJoin(thread, nullptr) == 0);
    Require(sceKernelDeleteSema(sem) == 0);
}

static void ExpectDelivery(int before, std::thread::id thread) {
    for (int attempt = 0; attempt < 5000 && calls.load() == before; ++attempt) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Require(calls.load() == before + 1);
    Require(handlerThread.load() == thread);
    Require(handlerRsp.load() != 0 && handlerFrame.load() < handlerRsp.load());
}

int main() {
    Require(sceKernelRaiseException(scePthreadSelf(), 11) == SCE_KERNEL_ERROR_EINVAL);
    bool rejected = false;
    try {
        sceKernelRaiseException(scePthreadSelf(), SIGUSR1);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Require(rejected);
    Require(sceKernelInstallExceptionHandler(SIGUSR1, reinterpret_cast<void*>(&Handler)) == 0);

    Require(sceKernelRaiseException(scePthreadSelf(), SIGUSR1) == 0);
    Require(calls.load() == 1 && handlerThread.load() == std::this_thread::get_id());

    Worker busy;
    Pthread busyThread = nullptr;
    Require(scePthreadCreate(&busyThread, nullptr, Busy, &busy, "busy") == 0);
    while (!busy.started.load()) std::this_thread::yield();
    for (int raised = 0; raised < Repeats; ++raised) {
        Require(sceKernelRaiseException(busyThread, SIGUSR1) == 0);
        ExpectDelivery(1 + raised, busy.id);
    }
    busy.stop.store(true);
    Require(scePthreadJoin(busyThread, nullptr) == 0);

    Worker waiting;
    Require(sceKernelCreateSema(&waiting.sem, "raise", 0, 0, 1, nullptr) == 0);
    Pthread waitingThread = nullptr;
    Require(scePthreadCreate(&waitingThread, nullptr, Waiting, &waiting, "waiting") == 0);
    while (!waiting.started.load()) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    for (int raised = 0; raised < Repeats; ++raised) {
        Require(sceKernelRaiseException(waitingThread, SIGUSR1) == 0);
        ExpectDelivery(1 + Repeats + raised, waiting.id);
    }
    Require(sceKernelSignalSema(waiting.sem, 1) == 0);
    Require(scePthreadJoin(waitingThread, nullptr) == 0);
    Require(waiting.waitResult == 0);
    Require(sceKernelDeleteSema(waiting.sem) == 0);

    Worker blocked;
    Pthread blockedThread = nullptr;
    hostLock.lock();
    Require(scePthreadCreate(&blockedThread, nullptr, HostBlocked, &blocked, "host blocked") == 0);
    for (int round = 0; round < HostRounds; ++round) {
        while (!blocked.started.load()) std::this_thread::yield();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        Require(sceKernelRaiseException(blockedThread, SIGUSR1) == 0);
        hostLock.unlock();
        ExpectDelivery(1 + 2 * Repeats + round, blocked.id);
        while (hostAcquired.load() != round + 1) std::this_thread::yield();
        hostLock.lock();
        blocked.started.store(false);
        hostRound.store(round + 1);
    }
    hostLock.unlock();
    Require(scePthreadJoin(blockedThread, nullptr) == 0);

    Worker leaving;
    Require(sceKernelCreateSema(&leaving.sem, "leaving", 0, 0, 1, nullptr) == 0);
    Pthread leavingThread = nullptr;
    Require(scePthreadCreate(&leavingThread, nullptr, Leaving, &leaving, "leaving") == 0);
    for (int round = 0; round < LeavingRounds; ++round) {
        while (!leaving.started.load()) std::this_thread::yield();
        leaving.started.store(false);
        std::this_thread::sleep_for(std::chrono::microseconds(200));
        Require(sceKernelSignalSema(leaving.sem, 1) == 0);
        for (int spin = 0; spin < round % 16 * 64; ++spin) std::this_thread::yield();
        Require(sceKernelRaiseException(leavingThread, SIGUSR1) == 0);
        ExpectDelivery(1 + 2 * Repeats + HostRounds + round, leaving.id);
        leavingRound.store(round + 1);
    }
    Require(scePthreadJoin(leavingThread, nullptr) == 0);
    Require(sceKernelDeleteSema(leaving.sem) == 0);

    if (__builtin_cpu_supports("avx")) {
        for (std::size_t i = 0; i < VectorBytes; ++i) vectorPattern[i] = static_cast<std::uint8_t>(i * 7 + 1);
        clobberVectors.store(true);
        Worker vectors;
        Pthread vectorsThread = nullptr;
        Require(scePthreadCreate(&vectorsThread, nullptr, Vectors, &vectors, "vectors") == 0);
        while (!vectors.started.load()) std::this_thread::yield();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        for (int raised = 0; raised < VectorRounds; ++raised) {
            Require(sceKernelRaiseException(vectorsThread, SIGUSR1) == 0);
            ExpectDelivery(1 + 2 * Repeats + HostRounds + LeavingRounds + raised, vectors.id);
        }
        clobberVectors.store(false);
        vectorStop = 1;
        Require(scePthreadJoin(vectorsThread, nullptr) == 0);
        Require(std::memcmp(vectorPattern, vectorResult, VectorBytes) == 0);
    }

    Pthread finishedThread = nullptr;
    Require(scePthreadCreate(&finishedThread, nullptr, Finished, nullptr, "finished") == 0);
    while (!finishedReturned.load()) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(sceKernelRaiseException(finishedThread, SIGUSR1) == SCE_KERNEL_ERROR_ESRCH);
    Require(scePthreadJoin(finishedThread, nullptr) == 0);

    Require(sceKernelRemoveExceptionHandler(SIGUSR1) == 0);

    Require(sceKernelInstallExceptionHandler(SIGUSR1, reinterpret_cast<void*>(&NestedHandler)) == 0);
    NestedWaitKeepsLaterWaiters();
    NestedWaitKeepsOuterWake();
    Require(sceKernelRemoveExceptionHandler(SIGUSR1) == 0);
}
