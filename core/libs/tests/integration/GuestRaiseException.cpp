#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <windows.h>

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

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;

constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);
constexpr int SCE_KERNEL_ERROR_ESRCH = static_cast<int>(0x80020003);
constexpr int SCE_KERNEL_ERROR_ETIMEDOUT = static_cast<int>(0x8002003c);
constexpr int SIGUSR1 = 30;
constexpr int repeats = 100;
constexpr int hostRounds = 20;
constexpr int leavingRounds = 200;
constexpr int continuingRounds = 50;
constexpr int selfingRounds = 100;
constexpr int vectorRounds = 100;
constexpr std::size_t vectorBytes = 16 * 32;
constexpr DWORD continuedCode = 0xe0000001u;

using ExceptionHandler = void (APS5_VABI *)(int, void*);
using NtContinueFunction = LONG(NTAPI*)(CONTEXT*, BOOLEAN);

struct Delivery {
    std::atomic<int> calls{0};
    std::atomic<int> wrongSignals{0};
    std::atomic<std::thread::id> thread;
    std::atomic<std::uint64_t> rsp{0};
    std::atomic<std::uintptr_t> frame{0};
    std::atomic<Pthread> self{nullptr};
    std::atomic<bool> clobberVectors{false};
};

struct NestedState {
    std::uint32_t word = 0;
    std::atomic<KernelUseconds> timeout{0};
    std::atomic<bool> entered{false};
    std::atomic<int> result{0};
    std::atomic<int> calls{0};
    std::atomic<int> wrongSignals{0};
};

std::atomic<Delivery*> activeDelivery{nullptr};
std::atomic<NestedState*> activeNested{nullptr};
std::atomic<std::atomic<int>*> continuedCounter{nullptr};

void APS5_VABI Handler(int signum, void* context) {
    Delivery* const state = activeDelivery.load();
    if (state == nullptr) return;
    if (signum != SIGUSR1) state->wrongSignals.fetch_add(1);
    std::uint64_t rsp = 0;
    std::memcpy(&rsp, static_cast<unsigned char*>(context) + 0xf8, sizeof(rsp));
    int local = 0;
    state->frame.store(reinterpret_cast<std::uintptr_t>(&local));
    state->rsp.store(rsp);
    state->thread.store(std::this_thread::get_id());
    state->self.store(scePthreadSelf());
    if (state->clobberVectors.load()) asm volatile("vzeroall" ::: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15");
    state->calls.fetch_add(1);
}

void APS5_VABI NestedHandler(int signum, void*) {
    NestedState* const state = activeNested.load();
    if (state == nullptr) return;
    if (signum != SIGUSR1) state->wrongSignals.fetch_add(1);
    state->entered.store(true);
    KernelUseconds timeout = state->timeout.load();
    state->result.store(sceKernelSyncOnAddressWait(&state->word, 0, &timeout, "nested"));
    state->calls.fetch_add(1);
}

template<typename TState>
class HandlerScope {
public:
    HandlerScope(std::atomic<TState*>& active, ExceptionHandler handler) : active(active) {
        RequireEqual(sceKernelInstallExceptionHandler(SIGUSR1, reinterpret_cast<void*>(handler)), 0, "install the handler");
        active.store(&state);
    }

    ~HandlerScope() {
        sceKernelRemoveExceptionHandler(SIGUSR1);
        active.store(nullptr);
    }

    HandlerScope(const HandlerScope&) = delete;
    HandlerScope& operator=(const HandlerScope&) = delete;

    int Remove() {
        return sceKernelRemoveExceptionHandler(SIGUSR1);
    }

    TState state;

private:
    std::atomic<TState*>& active;
};

class GuestThread {
public:
    GuestThread(PthreadEntry entry, void* argument, const char* name, std::function<void()> release)
        : release(std::move(release)) {
        RequireEqual(scePthreadCreate(&thread, nullptr, entry, argument, name), 0, std::string("create thread ") + name);
    }

    ~GuestThread() {
        if (thread == nullptr) return;
        release();
        scePthreadJoin(thread, nullptr);
    }

    GuestThread(const GuestThread&) = delete;
    GuestThread& operator=(const GuestThread&) = delete;

    Pthread Handle() const noexcept { return thread; }

    int Join() {
        const int result = scePthreadJoin(thread, nullptr);
        thread = nullptr;
        return result;
    }

private:
    Pthread thread = nullptr;
    std::function<void()> release;
};

class Semaphore {
public:
    Semaphore(const char* name, int initial, int maximum) {
        RequireEqual(sceKernelCreateSema(&handle, name, 0, initial, maximum, nullptr), 0, std::string("create semaphore ") + name);
    }

    ~Semaphore() {
        if (handle != nullptr) sceKernelDeleteSema(handle);
    }

    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;

    KernelSema Handle() const noexcept { return handle; }

    int Delete() {
        const int result = sceKernelDeleteSema(handle);
        handle = nullptr;
        return result;
    }

private:
    KernelSema handle = nullptr;
};

class VectoredHandlerScope {
public:
    VectoredHandlerScope(std::atomic<int>& counter, PVECTORED_EXCEPTION_HANDLER handler) {
        continuedCounter.store(&counter);
        handle = AddVectoredExceptionHandler(1, handler);
    }

    ~VectoredHandlerScope() {
        if (handle != nullptr) RemoveVectoredExceptionHandler(handle);
        continuedCounter.store(nullptr);
    }

    VectoredHandlerScope(const VectoredHandlerScope&) = delete;
    VectoredHandlerScope& operator=(const VectoredHandlerScope&) = delete;

    bool Installed() const noexcept { return handle != nullptr; }

    ULONG Remove() {
        const ULONG result = RemoveVectoredExceptionHandler(handle);
        handle = nullptr;
        return result;
    }

private:
    void* handle = nullptr;
};

struct Worker {
    std::atomic<bool> started{false};
    std::atomic<bool> stop{false};
    std::thread::id id;
};

struct WaitingWorker : Worker {
    KernelSema sem = nullptr;
    int waitResult = -1;
};

struct HostBlockedWorker : Worker {
    std::mutex lock;
    std::atomic<int> round{0};
    std::atomic<int> acquired{0};
};

struct LeavingWorker : Worker {
    KernelSema sem = nullptr;
    std::atomic<int> round{0};
    std::atomic<int> waitFailure{0};
};

struct SelfingWorker : Worker {
    std::atomic<Pthread> sink{nullptr};
};

struct LoopingWorker : Worker {
    alignas(16) CONTEXT loopContext;
    alignas(16) CONTEXT leaveContext;
    std::atomic<DWORD> threadId{0};
};

struct VectorWorker : Worker {
    alignas(32) std::uint8_t pattern[vectorBytes];
    alignas(32) std::uint8_t result[vectorBytes];
    volatile std::uint8_t stopFlag = 0;
};

struct FinishedWorker {
    std::atomic<bool> returned{false};
};

struct SemaWaiter {
    KernelSema sem = nullptr;
    std::atomic<bool> started{false};
    std::atomic<bool> returned{false};
    int result = -1;
};

void* APS5_VABI Busy(void* arg) {
    auto& worker = *static_cast<Worker*>(arg);
    worker.id = std::this_thread::get_id();
    worker.started.store(true);
    volatile std::uint64_t spins = 0;
    while (!worker.stop.load()) spins = spins + 1;
    return nullptr;
}

void* APS5_VABI Waiting(void* arg) {
    auto& worker = *static_cast<WaitingWorker*>(arg);
    worker.id = std::this_thread::get_id();
    worker.started.store(true);
    worker.waitResult = sceKernelWaitSema(worker.sem, 1, nullptr);
    return nullptr;
}

void* APS5_VABI HostBlocked(void* arg) {
    auto& worker = *static_cast<HostBlockedWorker*>(arg);
    worker.id = std::this_thread::get_id();
    for (int round = 0; round < hostRounds; ++round) {
        while (worker.round.load() != round) {
            if (worker.stop.load()) return nullptr;
            std::this_thread::yield();
        }
        worker.started.store(true);
        std::lock_guard guard(worker.lock);
        worker.acquired.store(round + 1);
    }
    return nullptr;
}

void* APS5_VABI Leaving(void* arg) {
    auto& worker = *static_cast<LeavingWorker*>(arg);
    worker.id = std::this_thread::get_id();
    for (int round = 0; round < leavingRounds; ++round) {
        worker.started.store(true);
        const int result = sceKernelWaitSema(worker.sem, 1, nullptr);
        if (result != 0) {
            worker.waitFailure.store(result);
            return nullptr;
        }
        volatile std::uint64_t spins = 0;
        while (worker.round.load() == round && !worker.stop.load()) spins = spins + 1;
        if (worker.stop.load()) return nullptr;
    }
    return nullptr;
}

LONG CALLBACK ContinueRaised(EXCEPTION_POINTERS* info) {
    if (info->ExceptionRecord->ExceptionCode != continuedCode) return EXCEPTION_CONTINUE_SEARCH;
    if (auto* const counter = continuedCounter.load()) counter->fetch_add(1);
    return EXCEPTION_CONTINUE_EXECUTION;
}

void* APS5_VABI Continuing(void* arg) {
    auto& worker = *static_cast<Worker*>(arg);
    worker.id = std::this_thread::get_id();
    worker.started.store(true);
    while (!worker.stop.load()) RaiseException(continuedCode, 0, 0, nullptr);
    return nullptr;
}

void* APS5_VABI Selfing(void* arg) {
    auto& worker = *static_cast<SelfingWorker*>(arg);
    worker.id = std::this_thread::get_id();
    worker.started.store(true);
    while (!worker.stop.load()) worker.sink.store(scePthreadSelf());
    return nullptr;
}

void* APS5_VABI Looping(void* arg) {
    auto& worker = *static_cast<LoopingWorker*>(arg);
    const auto ntContinue = reinterpret_cast<NtContinueFunction>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtContinue")));
    volatile bool left = false;
    RtlCaptureContext(&worker.leaveContext);
    if (left) return nullptr;
    left = true;
    worker.loopContext = worker.leaveContext;
    worker.loopContext.Rip = reinterpret_cast<DWORD64>(ntContinue);
    worker.loopContext.Rcx = reinterpret_cast<DWORD64>(&worker.loopContext);
    worker.loopContext.Rdx = 0;
    worker.threadId.store(GetCurrentThreadId());
    worker.started.store(true);
    ntContinue(&worker.loopContext, FALSE);
    return nullptr;
}

void* APS5_VABI Vectors(void* arg) {
    auto& worker = *static_cast<VectorWorker*>(arg);
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
        : [pattern] "r"(worker.pattern), [result] "r"(worker.result), [stop] "r"(&worker.stopFlag)
        : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15");
    return nullptr;
}

void* APS5_VABI Finished(void* arg) {
    static_cast<FinishedWorker*>(arg)->returned.store(true);
    return nullptr;
}

void* APS5_VABI WaitSemaOnce(void* arg) {
    auto& waiter = *static_cast<SemaWaiter*>(arg);
    waiter.started.store(true);
    waiter.result = sceKernelWaitSema(waiter.sem, 1, nullptr);
    waiter.returned.store(true);
    return nullptr;
}

void AwaitStarted(const std::atomic<bool>& started) {
    while (!started.load()) std::this_thread::yield();
}

void ExpectDelivery(const Delivery& state, int before, std::thread::id thread, const std::string& message) {
    for (int attempt = 0; attempt < 5000 && state.calls.load() == before; ++attempt) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    RequireEqual(state.calls.load(), before + 1, message + ": handler calls");
    RequireEqual(state.wrongSignals.load(), 0, message + ": handler calls with another signal");
    Require(state.thread.load() == thread, message + ": handler ran on the target thread");
    Require(state.rsp.load() != 0 && state.frame.load() < state.rsp.load(), message + ": handler frame lies below the interrupted stack");
}

std::string Round(const char* scenario, int round) {
    return std::string(scenario) + " round " + std::to_string(round);
}

void ExpectReturned(const SemaWaiter& waiter, const std::string& message) {
    for (int attempt = 0; attempt < 5000 && !waiter.returned.load(); ++attempt) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Require(waiter.returned.load(), message + ": waiter returned");
    RequireEqual(waiter.result, 0, message + ": wait result");
}

void ExpectNestedDone(const NestedState& state, int before) {
    for (int attempt = 0; attempt < 5000 && state.calls.load() == before; ++attempt) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    RequireEqual(state.calls.load(), before + 1, "nested handler calls");
    RequireEqual(state.wrongSignals.load(), 0, "nested handler calls with another signal");
    RequireEqual(state.result.load(), SCE_KERNEL_ERROR_ETIMEDOUT, "nested wait result");
}

std::string ReleaseLooping(LoopingWorker& worker) {
    const HANDLE handle = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, worker.threadId.load());
    if (handle == nullptr) return "open the looping thread";
    std::string error;
    if (SuspendThread(handle) == static_cast<DWORD>(-1)) {
        error = "suspend the looping thread";
    } else {
        CONTEXT stopped{};
        stopped.ContextFlags = CONTEXT_CONTROL;
        if (GetThreadContext(handle, &stopped) == 0) error = "read the looping thread context";
        worker.loopContext = worker.leaveContext;
        if (ResumeThread(handle) == static_cast<DWORD>(-1) && error.empty()) error = "resume the looping thread";
    }
    CloseHandle(handle);
    return error;
}

const Case unsupportedSignal{"RaiseException_UnsupportedSignal_FailsWithEinval", [] {
    RequireEqual(sceKernelRaiseException(scePthreadSelf(), 11), SCE_KERNEL_ERROR_EINVAL, "signal 11");
}};

const Case noHandler{"RaiseException_NoHandlerInstalled_Throws", [] {
    sceKernelRemoveExceptionHandler(SIGUSR1);
    RequireThrows<std::runtime_error>([] { sceKernelRaiseException(scePthreadSelf(), SIGUSR1); }, "raise without a handler");
}};

const Case currentThread{"RaiseException_CurrentThread_RunsHandlerSynchronously", [] {
    HandlerScope<Delivery> scope(activeDelivery, Handler);
    RequireEqual(sceKernelRaiseException(scePthreadSelf(), SIGUSR1), 0, "raise");
    RequireEqual(scope.state.calls.load(), 1, "handler calls");
    Require(scope.state.thread.load() == std::this_thread::get_id(), "handler ran on the current thread");
    RequireEqual(scope.Remove(), 0, "remove the handler");
}};

const Case busyThread{"RaiseException_BusyThread_DeliversEveryRaise", [] {
    HandlerScope<Delivery> scope(activeDelivery, Handler);
    Worker busy;
    GuestThread thread(Busy, &busy, "busy", [&busy] { busy.stop.store(true); });
    AwaitStarted(busy.started);
    for (int raised = 0; raised < repeats; ++raised) {
        RequireEqual(sceKernelRaiseException(thread.Handle(), SIGUSR1), 0, Round("busy raise", raised));
        ExpectDelivery(scope.state, raised, busy.id, Round("busy", raised));
    }
    busy.stop.store(true);
    RequireEqual(thread.Join(), 0, "join");
}};

const Case semaphoreWait{"RaiseException_ThreadWaitingOnSemaphore_DeliversAndKeepsTheWait", [] {
    HandlerScope<Delivery> scope(activeDelivery, Handler);
    Semaphore sem("raise", 0, 1);
    WaitingWorker waiting;
    waiting.sem = sem.Handle();
    GuestThread thread(Waiting, &waiting, "waiting", [&sem] { sceKernelSignalSema(sem.Handle(), 1); });
    AwaitStarted(waiting.started);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    for (int raised = 0; raised < repeats; ++raised) {
        RequireEqual(sceKernelRaiseException(thread.Handle(), SIGUSR1), 0, Round("waiting raise", raised));
        ExpectDelivery(scope.state, raised, waiting.id, Round("waiting", raised));
    }
    RequireEqual(sceKernelSignalSema(sem.Handle(), 1), 0, "signal");
    RequireEqual(thread.Join(), 0, "join");
    RequireEqual(waiting.waitResult, 0, "wait result");
    RequireEqual(sem.Delete(), 0, "delete semaphore");
}};

const Case hostMutex{"RaiseException_ThreadBlockedOnHostMutex_DeliversAfterUnlock", [] {
    HandlerScope<Delivery> scope(activeDelivery, Handler);
    HostBlockedWorker blocked;
    std::unique_lock hostLock(blocked.lock);
    GuestThread thread(HostBlocked, &blocked, "host blocked", [&blocked, &hostLock] {
        blocked.stop.store(true);
        if (hostLock.owns_lock()) hostLock.unlock();
    });
    for (int round = 0; round < hostRounds; ++round) {
        AwaitStarted(blocked.started);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        RequireEqual(sceKernelRaiseException(thread.Handle(), SIGUSR1), 0, Round("host blocked raise", round));
        hostLock.unlock();
        ExpectDelivery(scope.state, round, blocked.id, Round("host blocked", round));
        while (blocked.acquired.load() != round + 1) std::this_thread::yield();
        hostLock.lock();
        blocked.started.store(false);
        blocked.round.store(round + 1);
    }
    hostLock.unlock();
    RequireEqual(thread.Join(), 0, "join");
}};

const Case leavingWait{"RaiseException_ThreadLeavingSemaphoreWait_DeliversEveryRaise", [] {
    HandlerScope<Delivery> scope(activeDelivery, Handler);
    Semaphore sem("leaving", 0, 1);
    LeavingWorker leaving;
    leaving.sem = sem.Handle();
    GuestThread thread(Leaving, &leaving, "leaving", [&leaving, &sem] {
        leaving.stop.store(true);
        sceKernelSignalSema(sem.Handle(), 1);
    });
    for (int round = 0; round < leavingRounds; ++round) {
        while (!leaving.started.load()) {
            RequireEqual(leaving.waitFailure.load(), 0, Round("leaving wait", round));
            std::this_thread::yield();
        }
        leaving.started.store(false);
        std::this_thread::sleep_for(std::chrono::microseconds(200));
        RequireEqual(sceKernelSignalSema(sem.Handle(), 1), 0, Round("leaving signal", round));
        for (int spin = 0; spin < round % 16 * 64; ++spin) std::this_thread::yield();
        RequireEqual(sceKernelRaiseException(thread.Handle(), SIGUSR1), 0, Round("leaving raise", round));
        ExpectDelivery(scope.state, round, leaving.id, Round("leaving", round));
        leaving.round.store(round + 1);
    }
    RequireEqual(thread.Join(), 0, "join");
    RequireEqual(leaving.waitFailure.load(), 0, "wait result inside the thread");
    RequireEqual(sem.Delete(), 0, "delete semaphore");
}};

const Case continuingThread{"RaiseException_ThreadContinuingVectoredExceptions_DeliversEveryRaise", [] {
    HandlerScope<Delivery> scope(activeDelivery, Handler);
    std::atomic<int> continued{0};
    VectoredHandlerScope vectored(continued, ContinueRaised);
    Require(vectored.Installed(), "add the vectored exception handler");
    Worker continuing;
    GuestThread thread(Continuing, &continuing, "continuing", [&continuing] { continuing.stop.store(true); });
    while (!continuing.started.load() || continued.load() == 0) std::this_thread::yield();
    for (int raised = 0; raised < continuingRounds; ++raised) {
        RequireEqual(sceKernelRaiseException(thread.Handle(), SIGUSR1), 0, Round("continuing raise", raised));
        ExpectDelivery(scope.state, raised, continuing.id, Round("continuing", raised));
    }
    continuing.stop.store(true);
    RequireEqual(thread.Join(), 0, "join");
    Require(vectored.Remove() != 0, "remove the vectored exception handler");
}};

const Case selfingThread{"RaiseException_ThreadCallingPthreadSelf_HandlerSeesTheTargetThread", [] {
    HandlerScope<Delivery> scope(activeDelivery, Handler);
    SelfingWorker selfing;
    GuestThread thread(Selfing, &selfing, "selfing", [&selfing] { selfing.stop.store(true); });
    AwaitStarted(selfing.started);
    for (int raised = 0; raised < selfingRounds; ++raised) {
        RequireEqual(sceKernelRaiseException(thread.Handle(), SIGUSR1), 0, Round("selfing raise", raised));
        ExpectDelivery(scope.state, raised, selfing.id, Round("selfing", raised));
        Require(scope.state.self.load() == thread.Handle(), Round("selfing handler self", raised));
    }
    selfing.stop.store(true);
    RequireEqual(thread.Join(), 0, "join");
}};

const Case loopingThread{"RaiseException_ThreadLoopingInNtContinue_ThrowsAfterRetryingForOneSecond", [] {
    HandlerScope<Delivery> scope(activeDelivery, Handler);
    auto looping = std::make_unique<LoopingWorker>();
    LoopingWorker& worker = *looping;
    GuestThread thread(Looping, &worker, "looping", [&worker] {
        AwaitStarted(worker.started);
        ReleaseLooping(worker);
    });
    AwaitStarted(worker.started);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const int beforeLooping = scope.state.calls.load();
    const auto raisedAt = std::chrono::steady_clock::now();
    bool refused = false;
    try {
        sceKernelRaiseException(thread.Handle(), SIGUSR1);
    } catch (const std::runtime_error&) {
        refused = true;
    }
    const auto retried = std::chrono::steady_clock::now() - raisedAt;
    const auto releaseError = ReleaseLooping(worker);
    Require(releaseError.empty(), releaseError);
    RequireEqual(thread.Join(), 0, "join");
    Require(refused, "raise refused with runtime_error");
    Require(retried >= std::chrono::milliseconds(900) && retried < std::chrono::seconds(10),
            "retried for " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(retried).count()) + " ms");
    RequireEqual(scope.state.calls.load(), beforeLooping, "handler calls");
}};

const Case vectorRegisters{"RaiseException_ThreadHoldingYmmRegisters_PreservesThemAcrossDelivery", [] {
    if (!__builtin_cpu_supports("avx")) Testing::Skip("AVX is not supported");
    HandlerScope<Delivery> scope(activeDelivery, Handler);
    auto vectors = std::make_unique<VectorWorker>();
    VectorWorker& worker = *vectors;
    for (std::size_t index = 0; index < vectorBytes; ++index) worker.pattern[index] = static_cast<std::uint8_t>(index * 7 + 1);
    scope.state.clobberVectors.store(true);
    GuestThread thread(Vectors, &worker, "vectors", [&worker] { worker.stopFlag = 1; });
    AwaitStarted(worker.started);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    for (int raised = 0; raised < vectorRounds; ++raised) {
        RequireEqual(sceKernelRaiseException(thread.Handle(), SIGUSR1), 0, Round("vectors raise", raised));
        ExpectDelivery(scope.state, raised, worker.id, Round("vectors", raised));
    }
    scope.state.clobberVectors.store(false);
    worker.stopFlag = 1;
    RequireEqual(thread.Join(), 0, "join");
    Require(std::memcmp(worker.pattern, worker.result, vectorBytes) == 0, "ymm registers preserved");
}};

const Case finishedThread{"RaiseException_FinishedThread_FailsWithEsrch", [] {
    HandlerScope<Delivery> scope(activeDelivery, Handler);
    FinishedWorker finished;
    GuestThread thread(Finished, &finished, "finished", [] {});
    AwaitStarted(finished.returned);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    RequireEqual(sceKernelRaiseException(thread.Handle(), SIGUSR1), SCE_KERNEL_ERROR_ESRCH, "raise");
    RequireEqual(thread.Join(), 0, "join");
}};

const Case nestedLaterWaiters{"RaiseException_HandlerWaitTimesOut_KeepsLaterSemaphoreWaiters", [] {
    HandlerScope<NestedState> scope(activeNested, NestedHandler);
    Semaphore sem("nested later", 0, 2);
    SemaWaiter first;
    SemaWaiter second;
    first.sem = sem.Handle();
    second.sem = sem.Handle();
    const auto release = [&sem] { sceKernelSignalSema(sem.Handle(), 1); };
    GuestThread firstThread(WaitSemaOnce, &first, "nested first", release);
    AwaitStarted(first.started);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    GuestThread secondThread(WaitSemaOnce, &second, "nested second", release);
    AwaitStarted(second.started);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    scope.state.timeout.store(20000);
    const int before = scope.state.calls.load();
    RequireEqual(sceKernelRaiseException(firstThread.Handle(), SIGUSR1), 0, "raise");
    ExpectNestedDone(scope.state, before);
    RequireEqual(sceKernelSignalSema(sem.Handle(), 2), 0, "signal");
    ExpectReturned(first, "first waiter");
    ExpectReturned(second, "second waiter");
    RequireEqual(firstThread.Join(), 0, "join first");
    RequireEqual(secondThread.Join(), 0, "join second");
    RequireEqual(sem.Delete(), 0, "delete semaphore");
}};

const Case nestedOuterWake{"RaiseException_SignalDuringHandlerWait_KeepsTheOuterWake", [] {
    HandlerScope<NestedState> scope(activeNested, NestedHandler);
    Semaphore sem("nested outer", 0, 1);
    SemaWaiter waiter;
    waiter.sem = sem.Handle();
    GuestThread thread(WaitSemaOnce, &waiter, "nested outer", [&sem] { sceKernelSignalSema(sem.Handle(), 1); });
    AwaitStarted(waiter.started);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    scope.state.timeout.store(1000000);
    scope.state.entered.store(false);
    const int before = scope.state.calls.load();
    RequireEqual(sceKernelRaiseException(thread.Handle(), SIGUSR1), 0, "raise");
    AwaitStarted(scope.state.entered);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    RequireEqual(sceKernelSignalSema(sem.Handle(), 1), 0, "signal");
    ExpectNestedDone(scope.state, before);
    ExpectReturned(waiter, "waiter");
    RequireEqual(thread.Join(), 0, "join");
    RequireEqual(sem.Delete(), 0, "delete semaphore");
    RequireEqual(scope.Remove(), 0, "remove the handler");
}};

} // namespace
