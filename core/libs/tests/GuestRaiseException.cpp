#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstddef>
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
static constexpr std::size_t R12Offset = 0xa0;
static constexpr std::size_t R13Offset = 0xa8;
static constexpr std::size_t Xmm8Offset = 0x140 + 160 + 8 * 16;
static constexpr std::size_t MxcsrOffset = 0x140 + 24;
static constexpr std::uint32_t FlushToZero = 0x8000;
static constexpr std::uint32_t ReservedMxcsrBit = 0x10000;
static constexpr int StartingRounds = 50;
static constexpr std::uint64_t SpinMarker = 0x0123456789abcdefULL;
static constexpr std::uint64_t PatchedR12 = 0x5a5a1234abcd0001ULL;
static constexpr std::uint64_t PatchedXmm8 = 0x7e7e5678dcba0002ULL;

static void Require(bool value) { if (!value) std::abort(); }

static std::atomic<int> calls{0};
static std::atomic<std::thread::id> handlerThread;
static std::atomic<std::uint64_t> handlerRsp{0};
static std::atomic<std::uintptr_t> handlerFrame{0};
static std::atomic<bool> patchContext{false};
static std::atomic<std::uint64_t> observedR13{0};
static std::atomic<std::uint64_t> observedXmm8{0};
static std::atomic<std::uint32_t> observedMxcsr{0};
static std::atomic<bool> recordSelf{false};
static std::atomic<Pthread> handlerSelf{nullptr};

static void APS5_VABI Handler(int signum, void* context) {
    Require(signum == SIGUSR1);
    auto* bytes = static_cast<unsigned char*>(context);
    std::uint64_t rsp = 0;
    std::memcpy(&rsp, bytes + 0xf8, sizeof(rsp));
    if (patchContext.load()) {
        std::uint64_t value = 0;
        std::memcpy(&value, bytes + R13Offset, sizeof(value));
        observedR13.store(value);
        std::memcpy(&value, bytes + Xmm8Offset, sizeof(value));
        observedXmm8.store(value);
        std::uint32_t mxcsr = 0;
        std::memcpy(&mxcsr, bytes + MxcsrOffset, sizeof(mxcsr));
        observedMxcsr.store(mxcsr);
        mxcsr |= FlushToZero | ReservedMxcsrBit;
        std::memcpy(bytes + R12Offset, &PatchedR12, sizeof(PatchedR12));
        std::memcpy(bytes + Xmm8Offset, &PatchedXmm8, sizeof(PatchedXmm8));
        std::memcpy(bytes + MxcsrOffset, &mxcsr, sizeof(mxcsr));
    }
    if (recordSelf.load()) handlerSelf.store(scePthreadSelf());
    int local = 0;
    handlerFrame.store(reinterpret_cast<std::uintptr_t>(&local));
    handlerRsp.store(rsp);
    handlerThread.store(std::this_thread::get_id());
    calls.fetch_add(1);
}

struct SpinState {
    std::atomic<std::uint32_t> started{0};
    std::uint32_t padding = 0;
    std::uint64_t r12 = 0;
    std::uint64_t xmm8 = 0;
    std::uint32_t mxcsr = 0;
};

static_assert(offsetof(SpinState, r12) == 8 && offsetof(SpinState, xmm8) == 16 && offsetof(SpinState, mxcsr) == 24);

extern "C" void APS5_VABI RaiseTestSpinUntilR12(SpinState* state);
asm(".text\n"
    ".globl RaiseTestSpinUntilR12\n"
    "RaiseTestSpinUntilR12:\n"
    "    pushq %r12\n"
    "    pushq %r13\n"
    "    subq $8, %rsp\n"
    "    stmxcsr (%rsp)\n"
    "    xorl %r12d, %r12d\n"
    "    movabsq $0x0123456789abcdef, %r13\n"
    "    movq %r13, %xmm8\n"
    "    movl $1, (%rdi)\n"
    "1:\n"
    "    pause\n"
    "    testq %r12, %r12\n"
    "    jz 1b\n"
    "    movq %r12, 8(%rdi)\n"
    "    movq %xmm8, 16(%rdi)\n"
    "    stmxcsr 24(%rdi)\n"
    "    ldmxcsr (%rsp)\n"
    "    addq $8, %rsp\n"
    "    popq %r13\n"
    "    popq %r12\n"
    "    ret\n");

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

static std::atomic<std::thread::id> spinningId;

static void* APS5_VABI Spinning(void* arg) {
    spinningId.store(std::this_thread::get_id());
    RaiseTestSpinUntilR12(static_cast<SpinState*>(arg));
    return nullptr;
}

static std::atomic<bool> startingRelease{false};

static void* APS5_VABI Starting(void*) {
    while (!startingRelease.load()) std::this_thread::yield();
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

    SpinState spin;
    Pthread spinningThread = nullptr;
    Require(scePthreadCreate(&spinningThread, nullptr, Spinning, &spin, "spinning") == 0);
    while (spin.started.load() == 0) std::this_thread::yield();
    patchContext.store(true);
    Require(sceKernelRaiseException(spinningThread, SIGUSR1) == 0);
    ExpectDelivery(1 + 2 * Repeats + HostRounds + LeavingRounds, spinningId.load());
    Require(scePthreadJoin(spinningThread, nullptr) == 0);
    patchContext.store(false);
    Require(observedR13.load() == SpinMarker && observedXmm8.load() == SpinMarker);
    Require(spin.r12 == PatchedR12 && spin.xmm8 == PatchedXmm8);
    Require(spin.mxcsr == (observedMxcsr.load() | FlushToZero));

    recordSelf.store(true);
    for (int round = 0; round < StartingRounds; ++round) {
        const int before = calls.load();
        startingRelease.store(false);
        Pthread startingThread = nullptr;
        Require(scePthreadCreate(&startingThread, nullptr, Starting, nullptr, "starting") == 0);
        Require(sceKernelRaiseException(startingThread, SIGUSR1) == 0);
        for (int attempt = 0; attempt < 5000 && calls.load() == before; ++attempt) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        Require(calls.load() == before + 1);
        Require(handlerSelf.load() == startingThread);
        startingRelease.store(true);
        Require(scePthreadJoin(startingThread, nullptr) == 0);
    }
    recordSelf.store(false);

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
