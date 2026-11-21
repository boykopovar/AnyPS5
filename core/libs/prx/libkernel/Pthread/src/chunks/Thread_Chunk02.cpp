extern "C" {
#include "prx/libc/include/GuestGs.hpp"
#include "prx/libkernel/Pthread/include/ThreadLifecycle.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "../include/Pthread.hpp"
#include "../include/ThreadLifecycle.hpp"
#include "../include/WindowsThreadLocal.hpp"
#include "../include/Cancel.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/AsmMacros.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libc/include/CpuTopology.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <utility>
#include <vector>
#include <string>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <system_error>

#ifndef _WIN32
#include <alloca.h>
#include <cerrno>
#include <csetjmp>
#include <csignal>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach/mach.h>
#endif
#endif

#ifdef __linux__
#include <sys/syscall.h>
#else
static const auto initialThreadId = std::this_thread::get_id();
#endif

bool GuestThreadStackHostRangeOverlaps(std::uintptr_t start, std::uintptr_t end) {
#if defined(__linux__)
    std::lock_guard lock(guestStackMappingsLock);
    auto it = guestStackMappings.upper_bound(start);
    if (it != guestStackMappings.begin()) --it;
    for (; it != guestStackMappings.end() && it->first < end; ++it) {
        const auto mappingStart = it->first;
        const auto& stack = it->second;
        if (start < stack.stackStart && end > mappingStart) return true;
        if (start < stack.mappingEnd && end > stack.stackEnd) return true;
    }
#else
    (void)start;
    (void)end;
#endif
    return false;
}

struct CpuClockRegistry {
    std::mutex lock;
    std::map<int, PthreadPrivate*> threads;
    int nextThread = 100000;
    KernelCpumask processAffinity = DEFAULT_THREAD_AFFINITY;
};

static CpuClockRegistry& CpuClocks() {
    static auto* registry = new CpuClockRegistry();
    return *registry;
}

static bool GuestThreadEnded(const PthreadPrivate* thread) {
    if (thread->_finished.load(std::memory_order_acquire)) return true;
#ifdef _WIN32
    return thread->hostAdopted && WaitForSingleObject(static_cast<HANDLE>(thread->nativeHandle), 0) == WAIT_OBJECT_0;
#else
    return false;
#endif
}

PthreadPrivate::PthreadPrivate(const PthreadAttrPrivate* attr) : _finished(false), _retval(nullptr), _detached(false), _adopted(false), _handleStorage(nullptr), _joined(false) {
    auto& registry = CpuClocks();
    std::lock_guard lock(registry.lock);
    const auto* parent = CurrentGuestState().current;
    const KernelCpumask inherited = parent ? parent->affinity.load(std::memory_order_relaxed) : registry.processAffinity;
    affinity.store(attr && attr->_affinityExplicit ? attr->_affinity : inherited, std::memory_order_relaxed);
    constexpr int ExplicitSched = 0;
    const bool explicitSched = attr && attr->_inheritsched == ExplicitSched;
    if (attr) inheritSched = attr->_inheritsched;
    const int inheritedPolicy = parent ? parent->schedPolicy.load(std::memory_order_relaxed) : schedPolicy.load(std::memory_order_relaxed);
    schedPolicy.store(explicitSched ? attr->_schedpolicy : inheritedPolicy, std::memory_order_relaxed);
    const int attrPriority = attr ? attr->_schedpriority : DEFAULT_THREAD_PRIORITY;
    priority.store(explicitSched || !parent ? attrPriority : parent->priority.load(std::memory_order_relaxed), std::memory_order_relaxed);
    if (registry.nextThread > static_cast<int>(CPU_CLOCK_ID_MASK))
        throw std::runtime_error("Guest thread CPU clock identifiers are exhausted");
    cpuClockThread = registry.nextThread++;
    registry.threads.emplace(cpuClockThread, this);
}

std::unique_lock<std::mutex> LockMainGuestThread_nid_no_patch(PthreadPrivate*& thread) {
    auto& registry = CpuClocks();
    std::unique_lock lock(registry.lock);
    for (const auto& [id, candidate] : registry.threads) {
        if (!candidate->initialThread.load(std::memory_order_acquire) || GuestThreadEnded(candidate)) continue;
        thread = candidate;
        return lock;
    }
    return {};
}

PthreadPrivate::~PthreadPrivate() {
    auto& registry = CpuClocks();
    std::lock_guard lock(registry.lock);
    registry.threads.erase(cpuClockThread);
}

static void BindHostCpuClock(PthreadPrivate* self) {
#ifdef _WIN32
    (void)self;
#elif defined(__APPLE__)
    auto& registry = CpuClocks();
    std::lock_guard lock(registry.lock);
    self->hostMachThread = pthread_mach_thread_np(pthread_self());
    self->hostCpuClockBound = true;
#else
    clockid_t clock{};
    const int error = pthread_getcpuclockid(pthread_self(), &clock);
    if (error != 0)
        throw std::system_error(error, std::generic_category(), "Querying the guest thread CPU clock");
    auto& registry = CpuClocks();
    std::lock_guard lock(registry.lock);
    self->hostCpuClock = clock;
    self->hostCpuClockBound = true;
#endif
}

int GuestThreadCpuClockId(const PthreadPrivate* thread) {
    return static_cast<int>(CPU_CLOCK_BIT | static_cast<std::uint32_t>(thread->cpuClockThread));
}

void SetLiveGuestThreadsAffinity(KernelCpumask mask) {
    auto& registry = CpuClocks();
    std::lock_guard lock(registry.lock);
    registry.processAffinity = mask;
    for (const auto& [id, thread] : registry.threads) {
        if (!GuestThreadEnded(thread)) thread->affinity.store(mask, std::memory_order_relaxed);
    }
}

KernelCpumask LiveGuestThreadsAffinity() {
    auto& registry = CpuClocks();
    std::lock_guard lock(registry.lock);
    KernelCpumask mask = 0;
    for (const auto& [id, thread] : registry.threads) {
        if (!GuestThreadEnded(thread)) mask |= thread->affinity.load(std::memory_order_relaxed);
    }
    return mask;
}

std::unique_lock<std::mutex> LockLiveGuestThread(const PthreadPrivate* thread) {
    auto& registry = CpuClocks();
    std::unique_lock lock(registry.lock);
    for (const auto& [id, live] : registry.threads) {
        if (live != thread || GuestThreadEnded(live)) continue;
        return lock;
    }
    return {};
}

static std::unique_lock<std::mutex> LockRegisteredGuestThread(const PthreadPrivate* thread) {
    auto& registry = CpuClocks();
    std::unique_lock lock(registry.lock);
    for (const auto& [id, registered] : registry.threads) {
        if (registered == thread) return lock;
    }
    return {};
}

bool GuestThreadIsLive(const PthreadPrivate* thread) {
    return LockLiveGuestThread(thread).owns_lock();
}

extern "C" int ResumeGuestThread_nid_no_patch(PthreadPrivate* thread) {
    constexpr int GuestEsrch = 3;
    const auto live = LockLiveGuestThread(thread);
    if (!live.owns_lock()) return GuestEsrch;
    thread->suspended.store(false, std::memory_order_release);
    thread->suspended.notify_all();
    return 0;
}

extern "C" void ResumeAllGuestThreads_nid_no_patch() {
    const auto* self = CurrentGuestState().current;
    auto& registry = CpuClocks();
    std::lock_guard lock(registry.lock);
    for (const auto& [id, thread] : registry.threads) {
        if (thread == self) continue;
        thread->suspended.store(false, std::memory_order_release);
        thread->suspended.notify_all();
    }
}

bool GetLiveGuestThreadAffinity_nid_no_patch(std::int64_t tid, KernelCpumask& mask) {
    auto& registry = CpuClocks();
    std::lock_guard lock(registry.lock);
    for (const auto& [id, thread] : registry.threads) {
        if (thread->tid != tid || GuestThreadEnded(thread)) continue;
        mask = thread->affinity.load(std::memory_order_relaxed);
        return true;
    }
    return false;
}

bool GetLiveGuestThreadSched_nid_no_patch(std::int64_t tid, int& policy, int& priority) {
    auto& registry = CpuClocks();
    std::lock_guard lock(registry.lock);
    for (const auto& [id, thread] : registry.threads) {
        if (thread->tid != tid || GuestThreadEnded(thread)) continue;
        policy = thread->schedPolicy.load(std::memory_order_relaxed);
        priority = thread->priority.load(std::memory_order_relaxed);
        return true;
    }
    return false;
}

bool SetLiveGuestThreadAffinity_nid_no_patch(std::int64_t tid, KernelCpumask mask) {
    auto& registry = CpuClocks();
    std::lock_guard lock(registry.lock);
    for (const auto& [id, thread] : registry.threads) {
        if (thread->tid != tid || GuestThreadEnded(thread)) continue;
        thread->affinity.store(mask, std::memory_order_relaxed);
        return true;
    }
    return false;
}

bool LiveGuestThreadExists(std::int64_t tid) {
    auto& registry = CpuClocks();
    std::lock_guard lock(registry.lock);
    for (const auto& [id, thread] : registry.threads) {
        if (!GuestThreadEnded(thread) && thread->tid == tid) return true;
    }
    return false;
}

extern "C" bool LiveGuestThreadName_nid_no_patch(std::int64_t tid, char* name, std::size_t capacity) {
    auto& registry = CpuClocks();
    std::lock_guard lock(registry.lock);
    for (const auto& [id, thread] : registry.threads) {
        if (GuestThreadEnded(thread) || thread->tid != tid) continue;
        if (capacity == 0) return true;
        std::lock_guard nameGuard(thread->nameLock);
        const std::size_t length = std::min(thread->name.size(), capacity - 1);
        std::memcpy(name, thread->name.data(), length);
        name[length] = '\0';
        return true;
    }
    return false;
}

std::vector<LiveGuestThreadName> LiveGuestThreadNames() {
    scePthreadSelf();
    auto& registry = CpuClocks();
    std::lock_guard lock(registry.lock);
    std::vector<LiveGuestThreadName> threads;
    for (const auto& [id, thread] : registry.threads) {
        if (GuestThreadEnded(thread)) continue;
        std::lock_guard nameLock(thread->nameLock);
        threads.push_back({thread->tid, thread->name});
    }
    return threads;
}

bool GuestThreadCpuNanos(int cpuClockThread, std::uint64_t* nanos) {
    auto& registry = CpuClocks();
    std::lock_guard lock(registry.lock);
    const auto found = registry.threads.find(cpuClockThread);
    if (found == registry.threads.end())
        return false;
    PthreadPrivate* thread = found->second;
    if (GuestThreadEnded(thread))
        return false;
#ifdef _WIN32
    if (thread->nativeHandle == nullptr) {
        *nanos = 0;
        return true;
    }
    if (WaitForSingleObject(thread->nativeHandle, 0) == WAIT_OBJECT_0)
        return false;
    if (!HostThreadCpuNanos_nid_no_patch(thread->nativeHandle, nanos))
        throw std::system_error(GetLastError(), std::system_category(), "Querying guest thread CPU time");
#else
    if (!thread->hostCpuClockBound) {
        *nanos = 0;
        return true;
    }
#ifdef __APPLE__
    thread_basic_info_data_t info{};
    mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
    if (thread_info(thread->hostMachThread, THREAD_BASIC_INFO, reinterpret_cast<thread_info_t>(&info), &count) != KERN_SUCCESS)
        return false;
    const auto microseconds = (static_cast<std::uint64_t>(info.user_time.seconds) + static_cast<std::uint64_t>(info.system_time.seconds)) * 1000000ULL +
        static_cast<std::uint64_t>(info.user_time.microseconds) + static_cast<std::uint64_t>(info.system_time.microseconds);
    *nanos = microseconds * 1000ULL;
#else
    struct timespec time{};
    if (clock_gettime(thread->hostCpuClock, &time) != 0)
        throw std::system_error(errno, std::generic_category(), "Querying guest thread CPU time");
    *nanos = static_cast<std::uint64_t>(time.tv_sec) * 1000000000ULL + static_cast<std::uint64_t>(time.tv_nsec);
#endif
#endif
    return true;
}
#ifndef _WIN32
static thread_local std::unique_ptr<PthreadPrivate> adoptedThread;
// pthread_exit force-unwinds through guest frames, whose personality resolves to
// libc.prx's __gxx_personality_v0; that routine cannot read libgcc's unwind context
// and aborts. scePthreadExit instead jumps back to the thread start routine, which,
// like _endthreadex on Windows, skips the guest frames without unwinding them.
static thread_local std::jmp_buf* threadExitJump = nullptr;

static void* MeasureStackReserve(void* reserve) {
    std::uintptr_t low = 0;
    std::uintptr_t high = 0;
    if (HostStackLimits(&low, &high))
        *static_cast<std::size_t*>(reserve) = high - reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0));
    return nullptr;
}

static std::size_t HostStackReserve() {
    static const std::size_t reserve = [] {
        std::size_t measured = 0;
        pthread_t probe{};
        if (const int result = pthread_create(&probe, nullptr, MeasureStackReserve, &measured); result != 0)
            throw std::system_error(result, std::generic_category(), "Measuring the host thread stack reserve");
        if (const int result = pthread_join(probe, nullptr); result != 0)
            throw std::system_error(result, std::generic_category(), "Measuring the host thread stack reserve");
        if (measured == 0)
            throw std::runtime_error("Cannot measure the host thread stack reserve");
        return measured;
    }();
    return reserve;
}
#endif

void ThreadLifecycle::SetThreadDtors(thread_dtors_func_t callback) {
    if (!callback)
        throw std::runtime_error("Thread destructor callback is null");
    thread_dtors_func_t expected = nullptr;
    if (!threadDtors.compare_exchange_strong(expected, callback))
        throw std::runtime_error("Thread destructor callback is already registered");
}

void ThreadLifecycle::SetThreadAtexitCount(get_thread_atexit_count_func_t callback) {
    if (!callback)
        throw std::runtime_error("Thread atexit count callback is null");
    get_thread_atexit_count_func_t expected = nullptr;
    if (!threadAtexitCount.compare_exchange_strong(expected, callback))
        throw std::runtime_error("Thread atexit count callback is already registered");
}

void ThreadLifecycle::SetThreadAtexitReport(thread_atexit_report_func_t callback) {
    if (!callback)
        throw std::runtime_error("Thread atexit report callback is null");
    thread_atexit_report_func_t expected = nullptr;
    if (!threadAtexitReport.compare_exchange_strong(expected, callback))
        throw std::runtime_error("Thread atexit report callback is already registered");
}

static void finishThread(PthreadPrivate* self, void* retval) {
    if (!self || self != CurrentGuestState().current)
        throw std::runtime_error("Finishing an unregistered guest thread");
    if (CurrentGuestState().finishing)
        throw std::runtime_error("Guest thread is already finishing");
    CurrentGuestState().finishing = true;
    CurrentGuestState().exiting = true;
    UnregisterStack(self);
    if (const auto callback = threadDtors.load())
        callback();
    ThreadLifecycle::RunKeyDtors();
    {
        std::unique_lock<std::mutex> lk(self->_join_mtx);
        self->_retval = retval;
        self->_finished.store(true, std::memory_order_release);
    }
    self->_join_cv.NotifyAll();
#ifdef _WIN32
    TimedWait::ReapDeliveryDrain();
#endif
}

bool ThreadCancel::Requested() {
    return CurrentGuestState().current && !CurrentGuestState().exiting && CurrentGuestState().cancelState == GUEST_CANCEL_ENABLE && CurrentGuestState().current->cancelPending.load();
}

void ThreadCancel::Check() {
    if (Requested()) scePthreadExit(GUEST_CANCELED);
}

void* ThreadCancel::CleanupMark() {
    return CurrentGuestState().cleanupHandlers;
}

void ThreadCancel::CleanupRestore(void* mark) {
    CurrentGuestState().cleanupHandlers = static_cast<CleanupHandler*>(mark);
}

void ThreadCancel::BeginWait(TimedWait::Condition* condition) {
    auto& state = CurrentGuestState();
    if (!state.current) return;
    std::lock_guard lock(state.current->cancelLock);
    state.current->cancelWait = condition;
    state.current->cancelWaitExiting = condition && state.exiting;
}

void ThreadCancel::EndWait() {
    BeginWait(nullptr);
}

extern "C" void* APS5_VABI Aps5CallGuestEntry_nid_no_patch(PthreadEntry entry, void* arg);

asm(".text\n"
    APS5_ASM_FUNCTION("Aps5CallGuestEntry_nid_no_patch") R"(
    .cfi_startproc
    push %rbp
    .cfi_adjust_cfa_offset 8
    .cfi_rel_offset %rbp, 0
    push $0
    .cfi_adjust_cfa_offset 8
    push $0
    .cfi_adjust_cfa_offset 8
    mov %rsp, %rbp
    sub $16, %rsp
    .cfi_adjust_cfa_offset 16
    movl $0x9fc0, (%rsp)
    ldmxcsr (%rsp)
    mov %rdi, %rax
    mov %rsi, %rdi
    call *%rax
    add $32, %rsp
    .cfi_adjust_cfa_offset -32
    pop %rbp
    .cfi_adjust_cfa_offset -8
    .cfi_restore %rbp
    ret
    .cfi_endproc
)" APS5_ASM_FUNCTION_END("Aps5CallGuestEntry_nid_no_patch"));

static void RunThread(std::unique_ptr<ThreadArgs> args) {
    APS5_LOG_OUT("RunThread entry=0x%llx arg=%p self=%p", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(args->entry)), args->arg, static_cast<void*>(args->self));
    const auto entry = args->entry;
    void* arg = args->arg;
    PthreadPrivate* self = args->self;
    BindHostCpuClock(self);
    TimedWait::BindThreadWaitState(&self->waitCount);
    CurrentGuestState().current = self;
    asm volatile("fldcw %0" : : "m"(GUEST_THREAD_FPU_CONTROL));
    RegisterStack(self);
    sigprocmask_nid_postfix(3, &args->guestSignalMask, nullptr);
#ifndef _WIN32
    const sigset_t signalMask = args->signalMask;
#endif
#if defined(_WIN32) || defined(__APPLE__)
    GuestGsSet_nid_no_patch(args->gsBase);
#endif
    args.reset();
    self->suspended.wait(true, std::memory_order_acquire);
#ifdef _WIN32
    DeliverPendingException_nid_no_patch(self);
#else
#ifdef __APPLE__
    sigset_t hostMask = signalMask;
    KeepHostFaultSignalsUnblocked(hostMask);
    pthread_sigmask(SIG_SETMASK, &hostMask, nullptr);
#else
    pthread_sigmask(SIG_SETMASK, &signalMask, nullptr);
#endif
#endif
    finishThread(self, Aps5CallGuestEntry_nid_no_patch(entry, arg));
    asm volatile("ldmxcsr %0" : : "m"(self->hostMxcsr));
    CurrentGuestState().current = nullptr;
}

using FinalThreadReleaseTestHook = void (*)(Pthread);
static std::atomic<PthreadPrivate*> finalThreadReleaseTestTarget{nullptr};
static std::atomic<FinalThreadReleaseTestHook> finalThreadReleaseTestHook{nullptr};

}
