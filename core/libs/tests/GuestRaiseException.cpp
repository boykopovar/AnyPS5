#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Semaphore/include/Semaphore.hpp"
#include "prx/libkernel/Pthread/include/Rwlock.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

static constexpr int GUEST_SIGUSR1 = 30;
static constexpr int Repeats = 50;
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
static std::atomic<Pthread> handlerSelf{nullptr};
static std::atomic<bool> clobberVectors{false};
static std::atomic<bool> patchContext{false};
static std::atomic<std::uint64_t> observedR13{0};
static std::atomic<std::uint64_t> observedXmm8{0};
static std::atomic<std::uint32_t> observedMxcsr{0};

static void APS5_VABI Handler(int signum, void* context) {
    Require(signum == GUEST_SIGUSR1);
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
    int local = 0;
    handlerFrame.store(reinterpret_cast<std::uintptr_t>(&local));
    handlerRsp.store(rsp);
    handlerThread.store(std::this_thread::get_id());
    handlerSelf.store(scePthreadSelf());
    if (clobberVectors.load()) asm volatile("vzeroall" ::: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15");
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

static constexpr int LeavingRounds = 100;
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

#ifdef _WIN32
static constexpr DWORD ContinuedCode = 0xe0000001u;
static constexpr int ContinuingRounds = 50;
static std::atomic<int> continued{0};

static LONG CALLBACK ContinueRaised(EXCEPTION_POINTERS* info) {
    if (info->ExceptionRecord->ExceptionCode != ContinuedCode) return EXCEPTION_CONTINUE_SEARCH;
    continued.fetch_add(1);
    return EXCEPTION_CONTINUE_EXECUTION;
}

static constexpr int SelfingRounds = 50;
static std::atomic<Pthread> selfSink{nullptr};

static void* APS5_VABI Selfing(void* arg) {
    auto& worker = *static_cast<Worker*>(arg);
    worker.id = std::this_thread::get_id();
    worker.started.store(true);
    while (!worker.stop.load()) selfSink.store(scePthreadSelf());
    return nullptr;
}

static void* APS5_VABI Continuing(void* arg) {
    auto& worker = *static_cast<Worker*>(arg);
    worker.id = std::this_thread::get_id();
    worker.started.store(true);
    while (!worker.stop.load()) RaiseException(ContinuedCode, 0, 0, nullptr);
    return nullptr;
}

using NtContinueFunction = LONG(NTAPI*)(CONTEXT*, BOOLEAN);
alignas(16) static CONTEXT loopContext;
alignas(16) static CONTEXT leaveContext;
static std::atomic<DWORD> loopingId{0};

static void* APS5_VABI Looping(void* arg) {
    auto& worker = *static_cast<Worker*>(arg);
    const auto ntContinue = reinterpret_cast<NtContinueFunction>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtContinue")));
    volatile bool left = false;
    RtlCaptureContext(&leaveContext);
    if (left) return nullptr;
    left = true;
    loopContext = leaveContext;
    loopContext.Rip = reinterpret_cast<DWORD64>(ntContinue);
    loopContext.Rcx = reinterpret_cast<DWORD64>(&loopContext);
    loopContext.Rdx = 0;
    loopingId.store(GetCurrentThreadId());
    worker.started.store(true);
    ntContinue(&loopContext, FALSE);
    return nullptr;
}

static constexpr int VectorRounds = 50;
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
#endif

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
    Require(signum == GUEST_SIGUSR1);
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
    Require(sceKernelRaiseException(firstThread, GUEST_SIGUSR1) == 0);
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
    Require(sceKernelRaiseException(thread, GUEST_SIGUSR1) == 0);
    while (!nestedEntered.load()) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(sceKernelSignalSema(sem, 1) == 0);
    ExpectNestedDone(before);
    ExpectReturned(waiter);
    Require(scePthreadJoin(thread, nullptr) == 0);
    Require(sceKernelDeleteSema(sem) == 0);
}

static constexpr int AddressRounds = 50;
static std::uint32_t addressWord = 0;
static std::uint32_t otherAddressWord = 0;
static std::atomic<bool> holdingEntered{false};
static std::atomic<bool> holdingRelease{false};
static std::atomic<bool> holdingLeft{false};
static std::atomic<std::thread::id> holdingThread;

static void APS5_VABI HoldingHandler(int signum, void*) {
    Require(signum == GUEST_SIGUSR1);
    holdingThread.store(std::this_thread::get_id());
    holdingEntered.store(true);
    while (!holdingRelease.load()) std::this_thread::yield();
    holdingLeft.store(true);
}

static void* APS5_VABI AddressWaking(void* arg) {
    auto& worker = *static_cast<Worker*>(arg);
    worker.id = std::this_thread::get_id();
    worker.started.store(true);
    while (!worker.stop.load()) Require(sceKernelSyncOnAddressWake(&addressWord, 1) >= 0);
    return nullptr;
}

static void* APS5_VABI AddressWaiting(void* arg) {
    auto& worker = *static_cast<Worker*>(arg);
    worker.id = std::this_thread::get_id();
    worker.started.store(true);
    while (!worker.stop.load()) {
        KernelUseconds timeout = 20;
        const int result = sceKernelSyncOnAddressWait(&addressWord, 0, &timeout, "address waiting");
        Require(result == 0 || result == SCE_KERNEL_ERROR_ETIMEDOUT);
    }
    return nullptr;
}

static bool AwaitWithin(const std::atomic<bool>& flag, std::chrono::milliseconds limit) {
    const auto deadline = std::chrono::steady_clock::now() + limit;
    while (!flag.load()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::yield();
    }
    return true;
}

static void RequireInRound(bool value, const char* name, int round, const char* check) {
    if (value) return;
    std::fprintf(stderr, "%s round %d: %s\n", name, round, check);
    std::fflush(stderr);
    std::abort();
}

static void HandlerLeavesAddressLock(PthreadEntry entry, const char* name) {
    Worker worker;
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, entry, &worker, name) == 0);
    while (!worker.started.load()) std::this_thread::yield();
    for (int round = 0; round < AddressRounds; ++round) {
        holdingEntered.store(false);
        holdingLeft.store(false);
        holdingRelease.store(false);
        RequireInRound(sceKernelRaiseException(thread, GUEST_SIGUSR1) == 0, name, round, "the raise failed");
        RequireInRound(AwaitWithin(holdingEntered, std::chrono::seconds(5)), name, round, "the handler did not run within 5 s");
        RequireInRound(holdingThread.load() == worker.id, name, round, "the handler ran on another thread");
        std::atomic<bool> woke{false};
        std::thread waker([&] {
            Require(sceKernelSyncOnAddressWake(&otherAddressWord, 1) == 0);
            woke.store(true);
        });
        const bool returned = AwaitWithin(woke, std::chrono::milliseconds(500));
        holdingRelease.store(true);
        waker.join();
        RequireInRound(returned, name, round, "a wake on another address blocked for 500 ms while the handler ran");
        RequireInRound(AwaitWithin(holdingLeft, std::chrono::seconds(5)), name, round, "the handler did not return within 5 s of its release");
    }
    worker.stop.store(true);
    Require(scePthreadJoin(thread, nullptr) == 0);
}

static std::uint32_t idleWord = 0;

static void* APS5_VABI AddressIdle(void* arg) {
    auto& worker = *static_cast<Worker*>(arg);
    worker.id = std::this_thread::get_id();
    worker.started.store(true);
    worker.waitResult = sceKernelSyncOnAddressWait(&idleWord, 0, nullptr, "address idle");
    return nullptr;
}

static void HandlerRunsInUnboundedAddressWait() {
    Worker worker;
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, AddressIdle, &worker, "address idle") == 0);
    while (!worker.started.load()) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    holdingEntered.store(false);
    holdingLeft.store(false);
    holdingRelease.store(false);
    Require(sceKernelRaiseException(thread, GUEST_SIGUSR1) == 0);
    RequireInRound(AwaitWithin(holdingEntered, std::chrono::seconds(5)), "address idle", 0, "the handler did not run while the thread waited without a timeout");
    RequireInRound(holdingThread.load() == worker.id, "address idle", 0, "the handler ran on another thread");
    holdingRelease.store(true);
    RequireInRound(AwaitWithin(holdingLeft, std::chrono::seconds(5)), "address idle", 0, "the handler did not return within 5 s of its release");
    std::atomic_ref<std::uint32_t>(idleWord).store(1);
    Require(sceKernelSyncOnAddressWake(&idleWord, 1) >= 0);
    Require(scePthreadJoin(thread, nullptr) == 0);
    Require(worker.waitResult == 0);
}

static void ExpectDelivery(int before, std::thread::id thread) {
    for (int attempt = 0; attempt < 5000 && calls.load() == before; ++attempt) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Require(calls.load() == before + 1);
    Require(handlerThread.load() == thread);
    Require(handlerRsp.load() != 0 && handlerFrame.load() < handlerRsp.load());
}

static constexpr int LockRounds = 10;
static PthreadMutex guestMutex = nullptr;
static PthreadRwlock guestRwlock = nullptr;

static int LockMutex() { return scePthreadMutexLock(&guestMutex); }
static int TimedLockMutex() { return scePthreadMutexTimedlock(&guestMutex, 10000000); }
static int UnlockMutex() { return scePthreadMutexUnlock(&guestMutex); }
static int LockRwlock() { return scePthreadRwlockWrlock(&guestRwlock); }
static int UnlockRwlock() { return scePthreadRwlockUnlock(&guestRwlock); }

struct LockWorker {
    int (*lock)() = nullptr;
    int (*unlock)() = nullptr;
    std::atomic<int> blocking{0};
    std::atomic<int> acquired{0};
    std::atomic<int> released{0};
    std::thread::id id;
};

static void* APS5_VABI LockBlocked(void* arg) {
    auto& worker = *static_cast<LockWorker*>(arg);
    worker.id = std::this_thread::get_id();
    for (int round = 1; round <= LockRounds; ++round) {
        while (worker.released.load() != round - 1) std::this_thread::yield();
        worker.blocking.store(round);
        Require(worker.lock() == 0);
        worker.acquired.store(round);
        Require(worker.unlock() == 0);
    }
    return nullptr;
}

static void ExpectDeliveryWhileLockHeld(int (*lock)(), int (*unlock)(), int before) {
    LockWorker worker;
    worker.lock = lock;
    worker.unlock = unlock;
    Require(lock() == 0);
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, LockBlocked, &worker, "lock blocked") == 0);
    for (int round = 1; round <= LockRounds; ++round) {
        while (worker.blocking.load() != round) std::this_thread::yield();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        Require(sceKernelRaiseException(thread, GUEST_SIGUSR1) == 0);
        ExpectDelivery(before + round - 1, worker.id);
        Require(worker.acquired.load() == round - 1);
        Require(unlock() == 0);
        while (worker.acquired.load() != round) std::this_thread::yield();
        Require(lock() == 0);
        worker.released.store(round);
    }
    Require(unlock() == 0);
    Require(scePthreadJoin(thread, nullptr) == 0);
}

int main() {
    Require(sceKernelRaiseException(scePthreadSelf(), 11) == SCE_KERNEL_ERROR_EINVAL);
    bool rejected = false;
    try {
        sceKernelRaiseException(scePthreadSelf(), GUEST_SIGUSR1);

    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Require(rejected);
    Require(sceKernelInstallExceptionHandler(GUEST_SIGUSR1, reinterpret_cast<void*>(&Handler)) == 0);

    Require(sceKernelRaiseException(scePthreadSelf(), GUEST_SIGUSR1) == 0);
    Require(calls.load() == 1 && handlerThread.load() == std::this_thread::get_id());

    Worker busy;
    Pthread busyThread = nullptr;
    Require(scePthreadCreate(&busyThread, nullptr, Busy, &busy, "busy") == 0);
    while (!busy.started.load()) std::this_thread::yield();
    for (int raised = 0; raised < Repeats; ++raised) {
        Require(sceKernelRaiseException(busyThread, GUEST_SIGUSR1) == 0);
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
        Require(sceKernelRaiseException(waitingThread, GUEST_SIGUSR1) == 0);
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
        Require(sceKernelRaiseException(blockedThread, GUEST_SIGUSR1) == 0);
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
        const auto offset = std::chrono::steady_clock::now() + std::chrono::microseconds(round % 16 * 8);
        while (std::chrono::steady_clock::now() < offset) {}
        Require(sceKernelRaiseException(leavingThread, GUEST_SIGUSR1) == 0);
        ExpectDelivery(1 + 2 * Repeats + HostRounds + round, leaving.id);
        leavingRound.store(round + 1);
    }
    Require(scePthreadJoin(leavingThread, nullptr) == 0);
    Require(sceKernelDeleteSema(leaving.sem) == 0);

#ifdef _WIN32
    void* const vectored = AddVectoredExceptionHandler(1, ContinueRaised);
    Require(vectored != nullptr);
    Worker continuing;
    Pthread continuingThread = nullptr;
    Require(scePthreadCreate(&continuingThread, nullptr, Continuing, &continuing, "continuing") == 0);
    while (!continuing.started.load() || continued.load() == 0) std::this_thread::yield();
    for (int raised = 0; raised < ContinuingRounds; ++raised) {
        Require(sceKernelRaiseException(continuingThread, GUEST_SIGUSR1) == 0);
        ExpectDelivery(1 + 2 * Repeats + HostRounds + LeavingRounds + raised, continuing.id);
    }
    continuing.stop.store(true);
    Require(scePthreadJoin(continuingThread, nullptr) == 0);
    Require(RemoveVectoredExceptionHandler(vectored) != 0);

    Worker selfing;
    Pthread selfingThread = nullptr;
    Require(scePthreadCreate(&selfingThread, nullptr, Selfing, &selfing, "selfing") == 0);
    while (!selfing.started.load()) std::this_thread::yield();
    for (int raised = 0; raised < SelfingRounds; ++raised) {
        Require(sceKernelRaiseException(selfingThread, GUEST_SIGUSR1) == 0);
        ExpectDelivery(1 + 2 * Repeats + HostRounds + LeavingRounds + ContinuingRounds + raised, selfing.id);
        Require(handlerSelf.load() == selfingThread);
    }
    selfing.stop.store(true);
    Require(scePthreadJoin(selfingThread, nullptr) == 0);

    Worker looping;
    Pthread loopingThread = nullptr;
    Require(scePthreadCreate(&loopingThread, nullptr, Looping, &looping, "looping") == 0);
    while (!looping.started.load()) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const int beforeLooping = calls.load();
    const auto raisedAt = std::chrono::steady_clock::now();
    bool refused = false;
    try {
        sceKernelRaiseException(loopingThread, GUEST_SIGUSR1);

    } catch (const std::runtime_error&) {
        refused = true;
    }
    const auto retried = std::chrono::steady_clock::now() - raisedAt;
    Require(refused && retried >= std::chrono::milliseconds(900) && retried < std::chrono::seconds(10));
    Require(calls.load() == beforeLooping);
    const HANDLE loopingHandle = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, loopingId.load());
    Require(loopingHandle != nullptr);
    Require(SuspendThread(loopingHandle) != static_cast<DWORD>(-1));
    CONTEXT stopped{};
    stopped.ContextFlags = CONTEXT_CONTROL;
    Require(GetThreadContext(loopingHandle, &stopped) != 0);
    loopContext = leaveContext;
    Require(ResumeThread(loopingHandle) != static_cast<DWORD>(-1));
    CloseHandle(loopingHandle);
    Require(scePthreadJoin(loopingThread, nullptr) == 0);

    if (__builtin_cpu_supports("avx")) {
        for (std::size_t i = 0; i < VectorBytes; ++i) vectorPattern[i] = static_cast<std::uint8_t>(i * 7 + 1);
        clobberVectors.store(true);
        Worker vectors;
        Pthread vectorsThread = nullptr;
        Require(scePthreadCreate(&vectorsThread, nullptr, Vectors, &vectors, "vectors") == 0);
        while (!vectors.started.load()) std::this_thread::yield();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        for (int raised = 0; raised < VectorRounds; ++raised) {
            Require(sceKernelRaiseException(vectorsThread, GUEST_SIGUSR1) == 0);
            ExpectDelivery(1 + 2 * Repeats + HostRounds + LeavingRounds + ContinuingRounds + SelfingRounds + raised, vectors.id);
        }
        clobberVectors.store(false);
        vectorStop = 1;
        Require(scePthreadJoin(vectorsThread, nullptr) == 0);
        Require(std::memcmp(vectorPattern, vectorResult, VectorBytes) == 0);
    }
#endif

    SpinState spin;
    Pthread spinningThread = nullptr;
    Require(scePthreadCreate(&spinningThread, nullptr, Spinning, &spin, "spinning") == 0);
    while (spin.started.load() == 0) std::this_thread::yield();
    patchContext.store(true);
    const int spinBefore = calls.load();
    Require(sceKernelRaiseException(spinningThread, GUEST_SIGUSR1) == 0);
    ExpectDelivery(spinBefore, spinningId.load());
    Require(scePthreadJoin(spinningThread, nullptr) == 0);
    patchContext.store(false);
    Require(observedR13.load() == SpinMarker && observedXmm8.load() == SpinMarker);
    Require(spin.r12 == PatchedR12 && spin.xmm8 == PatchedXmm8);
    Require(spin.mxcsr == (observedMxcsr.load() | FlushToZero));

    for (int round = 0; round < StartingRounds; ++round) {
        const int before = calls.load();
        startingRelease.store(false);
        Pthread startingThread = nullptr;
        Require(scePthreadCreate(&startingThread, nullptr, Starting, nullptr, "starting") == 0);
        Require(sceKernelRaiseException(startingThread, GUEST_SIGUSR1) == 0);
        for (int attempt = 0; attempt < 5000 && calls.load() == before; ++attempt) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        Require(calls.load() == before + 1);
        Require(handlerSelf.load() == startingThread);
        startingRelease.store(true);
        Require(scePthreadJoin(startingThread, nullptr) == 0);
    }

    const int beforeLocks = calls.load();
    Require(scePthreadMutexInit(&guestMutex, nullptr, "raise") == 0);
    ExpectDeliveryWhileLockHeld(LockMutex, UnlockMutex, beforeLocks);
    ExpectDeliveryWhileLockHeld(TimedLockMutex, UnlockMutex, beforeLocks + LockRounds);
    Require(scePthreadMutexDestroy(&guestMutex) == 0);
    Require(scePthreadRwlockInit(&guestRwlock, nullptr, "raise") == 0);
    ExpectDeliveryWhileLockHeld(LockRwlock, UnlockRwlock, beforeLocks + 2 * LockRounds);
    Require(scePthreadRwlockDestroy(&guestRwlock) == 0);

    Pthread finishedThread = nullptr;
    Require(scePthreadCreate(&finishedThread, nullptr, Finished, nullptr, "finished") == 0);
    while (!finishedReturned.load()) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(sceKernelRaiseException(finishedThread, GUEST_SIGUSR1) == SCE_KERNEL_ERROR_ESRCH);
    Require(scePthreadJoin(finishedThread, nullptr) == 0);

    Require(sceKernelRemoveExceptionHandler(GUEST_SIGUSR1) == 0);

    Require(sceKernelInstallExceptionHandler(GUEST_SIGUSR1, reinterpret_cast<void*>(&NestedHandler)) == 0);
    NestedWaitKeepsLaterWaiters();
    NestedWaitKeepsOuterWake();
    Require(sceKernelRemoveExceptionHandler(GUEST_SIGUSR1) == 0);

    Require(sceKernelInstallExceptionHandler(GUEST_SIGUSR1, reinterpret_cast<void*>(&HoldingHandler)) == 0);
    HandlerLeavesAddressLock(AddressWaking, "address waking");
    HandlerLeavesAddressLock(AddressWaiting, "address waiting");
    HandlerRunsInUnboundedAddressWait();
    Require(sceKernelRemoveExceptionHandler(GUEST_SIGUSR1) == 0);
}
