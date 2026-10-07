#include "../include/Pthread.hpp"
#include "../include/ThreadLifecycle.hpp"
#include "../include/WindowsThreadLocal.hpp"
#include "../include/Cancel.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libc/include/CpuTopology.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <utility>
#include <string>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <system_error>

#ifndef _WIN32
#include <cerrno>
#include <csetjmp>
#include <pthread.h>
#include <unistd.h>
#endif

static constexpr int SCE_OK = 0;

static constexpr std::size_t DEFAULT_STACK_SIZE = 1u << 20;
static constexpr int DETACH_DETACHED = 1;
static constexpr std::size_t THREAD_NAME_CAPACITY = 32;

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#endif

struct ThreadArgs {
    PthreadEntry entry;
    void* arg;
    PthreadPrivate* self;
};

static std::atomic<thread_dtors_func_t> threadDtors{nullptr};
static std::atomic<get_thread_atexit_count_func_t> threadAtexitCount{nullptr};
static std::atomic<thread_atexit_report_func_t> threadAtexitReport{nullptr};
struct CleanupHandler {
    CleanupHandler* previous;
    void (APS5_VABI* routine)(void*);
    void* argument;
    int onHeap;
};

struct GuestThreadState {
    PthreadPrivate* current = nullptr;
    bool finishing = false;
    bool exiting = false;
    int cancelState = GUEST_CANCEL_ENABLE;
    int cancelType = GUEST_CANCEL_DEFERRED;
    CleanupHandler* cleanupHandlers = nullptr;
};
static GuestThreadState& CurrentGuestState() {
#ifdef _WIN32
    return WindowsThreadLocal<GuestThreadState>::Get();
#else
    static thread_local GuestThreadState state;
    return state;
#endif
}
static std::mutex stackLock;
static std::map<PthreadPrivate*, std::pair<std::uintptr_t, std::uintptr_t>> liveStacks;

static bool HostStackLimits(std::uintptr_t* low, std::uintptr_t* high) {
#ifdef _WIN32
    ULONG_PTR lowLimit = 0;
    ULONG_PTR highLimit = 0;
    GetCurrentThreadStackLimits(&lowLimit, &highLimit);
    *low = lowLimit;
    *high = highLimit;
#else
    pthread_attr_t attr;
    if (pthread_getattr_np(pthread_self(), &attr) != 0)
        return false;
    void* address = nullptr;
    std::size_t size = 0;
    const bool queried = pthread_attr_getstack(&attr, &address, &size) == 0;
    pthread_attr_destroy(&attr);
    if (!queried)
        return false;
    *low = reinterpret_cast<std::uintptr_t>(address);
    *high = *low + size;
#endif
    return *high > *low;
}

static void SetStackFromHost(PthreadPrivate* thread) {
    std::uintptr_t low = 0;
    std::uintptr_t high = 0;
    if (!HostStackLimits(&low, &high)) throw std::runtime_error("Cannot query the host thread stack");
    thread->stackAddress = reinterpret_cast<void*>(low);
    thread->stackSize = static_cast<std::size_t>(high - low);
}

static bool CurrentStack(std::uintptr_t* low, std::uintptr_t* high) {
    if (CurrentGuestState().current && CurrentGuestState().current->stackAddress) {
        *low = reinterpret_cast<std::uintptr_t>(CurrentGuestState().current->stackAddress);
        *high = *low + CurrentGuestState().current->stackSize;
        return true;
    }
    return HostStackLimits(low, high);
}

static void RegisterStack(PthreadPrivate* self) {
    std::uintptr_t low = 0;
    std::uintptr_t high = 0;
    if (!CurrentStack(&low, &high))
        throw std::runtime_error("Cannot query guest thread stack");
    std::lock_guard lock(stackLock);
    liveStacks[self] = {low, high};
}

static void UnregisterStack(PthreadPrivate* self) {
    std::lock_guard lock(stackLock);
    liveStacks.erase(self);
}

bool GuestThreadStack(std::uintptr_t address, std::uintptr_t* start, std::uintptr_t* end) {
    std::uintptr_t low = 0;
    std::uintptr_t high = 0;
    if (CurrentStack(&low, &high) && address >= low && address < high) {
        *start = low;
        *end = high;
        return true;
    }
    std::lock_guard lock(stackLock);
    for (const auto& [thread, range] : liveStacks) {
        if (address >= range.first && address < range.second) {
            *start = range.first;
            *end = range.second;
            return true;
        }
    }
    return false;
}

struct CpuClockRegistry {
    std::mutex lock;
    std::map<int, PthreadPrivate*> threads;
    int nextThread = 100000;
};

static CpuClockRegistry& CpuClocks() {
    static auto* registry = new CpuClockRegistry();
    return *registry;
}

PthreadPrivate::PthreadPrivate() : _finished(false), _retval(nullptr), _detached(false), _adopted(false) {
    auto& registry = CpuClocks();
    std::lock_guard lock(registry.lock);
    if (registry.nextThread > static_cast<int>(CPU_CLOCK_ID_MASK))
        throw std::runtime_error("Guest thread CPU clock identifiers are exhausted");
    cpuClockThread = registry.nextThread++;
    registry.threads.emplace(cpuClockThread, this);
}

PthreadPrivate::~PthreadPrivate() {
    auto& registry = CpuClocks();
    std::lock_guard lock(registry.lock);
    registry.threads.erase(cpuClockThread);
}

static void BindHostCpuClock(PthreadPrivate* self) {
#ifdef _WIN32
    (void)self;
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

bool GuestThreadCpuNanos(int cpuClockThread, std::uint64_t* nanos) {
    auto& registry = CpuClocks();
    std::lock_guard lock(registry.lock);
    const auto found = registry.threads.find(cpuClockThread);
    if (found == registry.threads.end())
        return false;
    PthreadPrivate* thread = found->second;
    if (thread->_finished.load(std::memory_order_acquire))
        return false;
#ifdef _WIN32
    if (thread->nativeHandle == nullptr) {
        *nanos = 0;
        return true;
    }
    if (WaitForSingleObject(thread->nativeHandle, 0) == WAIT_OBJECT_0)
        return false;
    FILETIME creation{};
    FILETIME exitTime{};
    FILETIME kernel{};
    FILETIME user{};
    if (!GetThreadTimes(thread->nativeHandle, &creation, &exitTime, &kernel, &user))
        throw std::system_error(GetLastError(), std::system_category(), "Querying guest thread CPU time");
    const auto toTicks = [](const FILETIME& time) {
        return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
    };
    *nanos = (toTicks(user) + toTicks(kernel)) * 100ULL;
#else
    if (!thread->hostCpuClockBound) {
        *nanos = 0;
        return true;
    }
    struct timespec time{};
    if (clock_gettime(thread->hostCpuClock, &time) != 0)
        throw std::system_error(errno, std::generic_category(), "Querying guest thread CPU time");
    *nanos = static_cast<std::uint64_t>(time.tv_sec) * 1000000000ULL + static_cast<std::uint64_t>(time.tv_nsec);
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
        *static_cast<std::size_t*>(reserve) = high - reinterpret_cast<std::uintptr_t>(&low);
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
    UnregisterStack(self);
    if (const auto callback = threadDtors.load())
        callback();
    {
        std::unique_lock<std::mutex> lk(self->_join_mtx);
        self->_retval = retval;
        self->_finished.store(true, std::memory_order_release);
    }
    self->_join_cv.NotifyAll();
}

bool ThreadCancel::Requested() {
    return CurrentGuestState().current && !CurrentGuestState().exiting && CurrentGuestState().cancelState == GUEST_CANCEL_ENABLE && CurrentGuestState().current->cancelPending.load();
}

void ThreadCancel::Check() {
    if (Requested()) scePthreadExit(GUEST_CANCELED);
}

void ThreadCancel::BeginWait(TimedWait::Condition* condition) {
    if (!CurrentGuestState().current) return;
    std::lock_guard lock(CurrentGuestState().current->cancelLock);
    CurrentGuestState().current->cancelWait = condition;
}

void ThreadCancel::EndWait() {
    BeginWait(nullptr);
}

static void RunThread(std::unique_ptr<ThreadArgs> args) {
    APS5_LOG_OUT("RunThread entry=0x%llx arg=%p self=%p", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(args->entry)), args->arg, static_cast<void*>(args->self));
    const auto entry = args->entry;
    void* arg = args->arg;
    PthreadPrivate* self = args->self;
    BindHostCpuClock(self);
    TimedWait::BindThreadWaitState(&self->waitCount);
    CurrentGuestState().current = self;
    RegisterStack(self);
    args.reset();
    finishThread(self, entry(arg));
    CurrentGuestState().current = nullptr;
}

static void ReleaseThread(PthreadPrivate* thread) {
    if (thread->_adopted)
        return;
    if (thread->references.fetch_sub(1, std::memory_order_acq_rel) != 1)
        return;
#ifdef _WIN32
    if (thread->wakeEvent)
        CloseHandle(static_cast<HANDLE>(thread->wakeEvent));
    if (!CloseHandle(thread->nativeHandle))
        throw std::system_error(GetLastError(), std::system_category(), "Closing guest thread handle");
#endif
    delete thread;
}

#ifdef _WIN32
// The title's job workers ("BPE JobWorkerThread CPU0".."CPU12") spin at full load on one core each.
// Pinning them to the efficiency cores of a hybrid CPU (APS5_JOB_AFFINITY=1, with the driver's
// worker pinning) measured 0 to -9 % at the video stage, so the default leaves them free;
// APS5_JOB_AFFINITY_MASK=<hex> picks the cores.
static constexpr const char* JOB_WORKER_PREFIX = "BPE JobWorkerThread";

static std::uint64_t JobWorkerMask() {
    static const std::uint64_t mask = [] {
        if (std::getenv("APS5_NO_JOB_AFFINITY") != nullptr) return std::uint64_t{0};
        const auto requested = CpuTopology::MaskFromEnvironment("APS5_JOB_AFFINITY_MASK");
        if (requested != 0) return requested;
        if (std::getenv("APS5_JOB_AFFINITY") == nullptr) return std::uint64_t{0};
        const auto& layout = CpuTopology::Get();
        return layout.hybrid ? layout.efficient : std::uint64_t{0};
    }();
    return mask;
}

static void ApplyJobAffinity(PthreadPrivate& thread, void* handle) {
    if (thread.name.compare(0, std::strlen(JOB_WORKER_PREFIX), JOB_WORKER_PREFIX) != 0) return;
    const auto mask = JobWorkerMask();
    if (mask == 0) return;
    static std::once_flag summary;
    std::call_once(summary, [mask] {
        const auto& layout = CpuTopology::Get();
        std::fprintf(stderr, "[affinity] job worker threads -> 0x%llx (hybrid=%d efficient=0x%llx performant=0x%llx process=0x%llx)\n", static_cast<unsigned long long>(mask), layout.hybrid ? 1 : 0, static_cast<unsigned long long>(layout.efficient), static_cast<unsigned long long>(layout.performant), static_cast<unsigned long long>(layout.process));
    });
    CpuTopology::PinTraced(thread.name.c_str(), handle, mask);
}
#endif

struct NativeThreadArgs {
    std::unique_ptr<ThreadArgs> guest;
    std::future<bool> start;
    std::promise<void> initialized;
};

#ifdef _WIN32
static unsigned __stdcall StartNativeThread(void* opaque) {
    std::unique_ptr<NativeThreadArgs> args(static_cast<NativeThreadArgs*>(opaque));
    auto* self = args->guest->self;
    try {
        ULONG_PTR low = 0;
        ULONG_PTR high = 0;
        GetCurrentThreadStackLimits(&low, &high);
        if (high <= low || high - low < self->stackSize)
            throw std::runtime_error("Cannot query guest thread stack");
        self->stackAddress = reinterpret_cast<void*>(high - self->stackSize);
        for (auto cursor = high - self->stackSize; cursor < high;) {
            MEMORY_BASIC_INFORMATION memory{};
            if (VirtualQuery(reinterpret_cast<void*>(cursor), &memory, sizeof(memory)) != sizeof(memory) || memory.State != MEM_COMMIT || memory.Protect != PAGE_READWRITE || memory.RegionSize == 0)
                throw std::runtime_error("Guest thread stack is not fully committed");
            cursor = reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize;
        }
        self->threadId = std::this_thread::get_id();
        CurrentGuestState().current = self;
        if (!self->name.empty()) {
            const std::wstring description(self->name.begin(), self->name.end());
            SetThreadDescription(GetCurrentThread(), description.c_str());
            ApplyJobAffinity(*self, nullptr);
        }
        args->initialized.set_value();
    } catch (...) {
        args->initialized.set_exception(std::current_exception());
        return 0;
    }
    if (!args->start.get())
        return 0;
    auto guest = std::move(args->guest);
    args.reset();
    RunThread(std::move(guest));
    CurrentGuestState().current = nullptr;
    ReleaseThread(self);
    return 0;
}
#else
static void* StartNativeThread(void* opaque) {
    std::unique_ptr<NativeThreadArgs> args(static_cast<NativeThreadArgs*>(opaque));
    auto* self = args->guest->self;
    try {
        std::uintptr_t low = 0;
        std::uintptr_t high = 0;
        const auto reserve = HostStackReserve();
        if (!HostStackLimits(&low, &high) || high - low < reserve + self->stackSize)
            throw std::runtime_error("Cannot query guest thread stack");
        self->stackAddress = reinterpret_cast<void*>(high - reserve - self->stackSize);
        self->threadId = std::this_thread::get_id();
        args->initialized.set_value();
    } catch (...) {
        args->initialized.set_exception(std::current_exception());
        return nullptr;
    }
    if (!args->start.get())
        return nullptr;
    auto* guest = args->guest.release();
    args.reset();
    struct ThreadGuard {
        PthreadPrivate* self;
        ~ThreadGuard() {
            CurrentGuestState().current = nullptr;
            ReleaseThread(self);
        }
    } guard{self};
    std::jmp_buf exitJump;
    if (setjmp(exitJump) == 0) {
        threadExitJump = &exitJump;
        RunThread(std::unique_ptr<ThreadArgs>(guest));
    }
    threadExitJump = nullptr;
    return nullptr;
}
#endif

extern "C" {

int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name) {
    if (!thread || !entry) throw std::runtime_error("scePthreadCreate: null arg");
    if (attr && !*attr) throw std::runtime_error("scePthreadCreate: null attributes");
    auto p = std::make_unique<PthreadPrivate>();
    bool detached = false;
    if (attr && *attr) detached = ((*attr)->_detachstate == DETACH_DETACHED);
    p->_detached = detached;
    p->stackSize = attr ? (*attr)->_stacksize : DEFAULT_STACK_SIZE;
    if (attr) {
        p->affinity.store((*attr)->_affinity, std::memory_order_relaxed);
        p->priority.store((*attr)->_schedpriority, std::memory_order_relaxed);
    }
    if (name) p->name = name;
    std::promise<bool> start;
    auto args = std::make_unique<ThreadArgs>(ThreadArgs{entry, arg, p.get()});
#ifdef _WIN32
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    const std::size_t nativeStack = (p->stackSize + system.dwPageSize - 1) / system.dwPageSize * system.dwPageSize;
    if (p->stackSize < 16384 || nativeStack > std::numeric_limits<unsigned>::max())
        throw std::runtime_error("scePthreadCreate: invalid Windows stack size");
    auto native = std::make_unique<NativeThreadArgs>(NativeThreadArgs{std::move(args), start.get_future(), {}});
    auto initialized = native->initialized.get_future();
    const auto handle = _beginthreadex(nullptr, static_cast<unsigned>(nativeStack), StartNativeThread, native.get(), 0, nullptr);
    if (handle == 0)
        throw std::system_error(errno, std::generic_category(), "Creating guest thread");
    p->nativeHandle = reinterpret_cast<void*>(handle);
    native.release();
    try {
        initialized.get();
    } catch (...) {
        start.set_value(false);
        WaitForSingleObject(p->nativeHandle, INFINITE);
        CloseHandle(p->nativeHandle);
        throw;
    }
    auto* published = p.release();
    *thread = published;
    start.set_value(true);
    if (detached)
        ReleaseThread(published);
#else
    const auto reserve = HostStackReserve();
    const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    if (p->stackSize < 16384 || p->stackSize > std::numeric_limits<std::size_t>::max() - reserve - page)
        throw std::runtime_error("scePthreadCreate: invalid stack size");
    const std::size_t nativeStack = (p->stackSize + reserve + page - 1) / page * page;
    auto native = std::make_unique<NativeThreadArgs>(NativeThreadArgs{std::move(args), start.get_future(), {}});
    auto initialized = native->initialized.get_future();
    pthread_attr_t nativeAttr;
    if (const int result = pthread_attr_init(&nativeAttr); result != 0)
        throw std::system_error(result, std::generic_category(), "Creating guest thread");
    int created = pthread_attr_setstacksize(&nativeAttr, nativeStack);
    if (created == 0)
        created = pthread_create(&p->hostThread, &nativeAttr, StartNativeThread, native.get());
    pthread_attr_destroy(&nativeAttr);
    if (created != 0)
        throw std::system_error(created, std::generic_category(), "Creating guest thread");
    native.release();
    try {
        initialized.get();
        if (detached) {
            if (const int result = pthread_detach(p->hostThread); result != 0)
                throw std::system_error(result, std::generic_category(), "Detaching guest thread");
        }
    } catch (...) {
        start.set_value(false);
        pthread_join(p->hostThread, nullptr);
        throw;
    }
    auto* published = p.release();
    *thread = published;
    start.set_value(true);
    if (detached)
        ReleaseThread(published);
#endif
    return SCE_OK;
}

int APS5_VABI scePthreadJoin(Pthread thread, void** retval) {
    if (!thread) throw std::runtime_error("scePthreadJoin: null thread");
    if (thread->_detached) return SCE_KERNEL_ERROR_EINVAL;
    if (thread == CurrentGuestState().current)
        throw std::runtime_error("scePthreadJoin: cannot join current thread");
    ThreadCancel::Check();
    {
        std::unique_lock lock(thread->_join_mtx);
        ThreadCancel::WaitUntil(thread->_join_cv, lock, std::nullopt, [&] { return thread->_finished.load(); });
    }
#ifdef _WIN32
    if (WaitForSingleObject(thread->nativeHandle, INFINITE) != WAIT_OBJECT_0)
        throw std::system_error(GetLastError(), std::system_category(), "Joining guest thread");
    if (retval) *retval = thread->_retval;
    ReleaseThread(thread);
#else
    if (const int result = pthread_join(thread->hostThread, nullptr); result != 0)
        throw std::system_error(result, std::generic_category(), "Joining guest thread");
    if (retval) *retval = thread->_retval;
    ReleaseThread(thread);
#endif
    return SCE_OK;
}

int APS5_VABI scePthreadDetach(Pthread thread) {
    if (!thread) throw std::runtime_error("scePthreadDetach: null thread");
    if (thread->_detached) return SCE_KERNEL_ERROR_EINVAL;
    thread->_detached = true;
#ifdef _WIN32
    ReleaseThread(thread);
#else
    if (const int result = pthread_detach(thread->hostThread); result != 0)
        throw std::system_error(result, std::generic_category(), "Detaching guest thread");
    ReleaseThread(thread);
#endif
    return SCE_OK;
}

void APS5_VABI scePthreadExit(void* retval) {
    if (!CurrentGuestState().current)
        throw std::runtime_error("scePthreadExit: current thread is not registered");
    auto* self = CurrentGuestState().current;
    CurrentGuestState().exiting = true;
    while (CurrentGuestState().cleanupHandlers) __pthread_cleanup_pop_imp_nid_postfix(1);
    finishThread(self, retval);
    CurrentGuestState().current = nullptr;
#ifdef _WIN32
    ReleaseThread(self);
    _endthreadex(0);
#else
    if (threadExitJump)
        std::longjmp(*threadExitJump, 1);
    pthread_exit(retval);
#endif
    throw std::runtime_error("Native thread exit returned");
}

Pthread APS5_VABI scePthreadSelf() {
#ifdef _WIN32
    if (!CurrentGuestState().current) {
        auto adopted = std::make_unique<PthreadPrivate>();
        HANDLE handle = nullptr;
        if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &handle, 0, FALSE, DUPLICATE_SAME_ACCESS))
            throw std::system_error(GetLastError(), std::system_category(), "Adopting guest thread");
        adopted->nativeHandle = handle;
        adopted->threadId = std::this_thread::get_id();
        adopted->_detached = true;
        adopted->references.store(1, std::memory_order_relaxed);
        TimedWait::BindThreadWaitState(&adopted->waitCount);
        SetStackFromHost(adopted.get());
        CurrentGuestState().current = adopted.release();
    }
#else
    if (!CurrentGuestState().current) {
        adoptedThread = std::make_unique<PthreadPrivate>();
        adoptedThread->_detached = true;
        adoptedThread->_adopted = true;
        BindHostCpuClock(adoptedThread.get());
        adoptedThread->threadId = std::this_thread::get_id();
        SetStackFromHost(adoptedThread.get());
        CurrentGuestState().current = adoptedThread.get();
    }
#endif
    return CurrentGuestState().current;
}

void APS5_VABI scePthreadYield() {
    std::this_thread::yield();
}

int APS5_VABI scePthreadCancel(Pthread thread) {
    if (!thread) return SCE_KERNEL_ERROR_ESRCH;
    thread->cancelPending.store(true);
    for (;;) {
        {
            std::lock_guard lock(thread->cancelLock);
            if (!thread->cancelWait) return SCE_OK;
            thread->cancelWait->NotifyAll();
        }
        std::this_thread::yield();
    }
}

void APS5_VABI __pthread_cleanup_push_imp_nid_postfix(void (APS5_VABI* routine)(void*), void* argument, void* info) {
    auto* handler = static_cast<CleanupHandler*>(info);
    handler->routine = routine;
    handler->argument = argument;
    handler->onHeap = 0;
    handler->previous = CurrentGuestState().cleanupHandlers;
    CurrentGuestState().cleanupHandlers = handler;
}

void APS5_VABI __pthread_cleanup_pop_imp_nid_postfix(int execute) {
    auto* handler = CurrentGuestState().cleanupHandlers;
    if (!handler) return;
    CurrentGuestState().cleanupHandlers = handler->previous;
    if (execute) handler->routine(handler->argument);
}

int APS5_VABI scePthreadEqual(Pthread thread1, Pthread thread2) {
    return thread1 == thread2 ? 1 : 0;
}

KernelCpumask APS5_VABI sceKernelGetAvailableCpumask(void) {
    return DEFAULT_THREAD_AFFINITY;
}

int APS5_VABI scePthreadGetaffinity(Pthread thread, KernelCpumask* mask) {
    if (!thread || !mask) return SCE_KERNEL_ERROR_EINVAL;
    *mask = thread->affinity.load(std::memory_order_relaxed);
    return SCE_OK;
}

int APS5_VABI scePthreadGetname(Pthread thread, char* name) {
    if (!thread || !name) return SCE_KERNEL_ERROR_EINVAL;
    std::lock_guard lock(thread->nameLock);
    const std::size_t length = std::min<std::size_t>(thread->name.size(), THREAD_NAME_CAPACITY - 1);
    std::memcpy(name, thread->name.data(), length);
    name[length] = '\0';
    return SCE_OK;
}

int APS5_VABI scePthreadGetprio(Pthread thread, int* prio) {
    if (!thread || !prio) return SCE_KERNEL_ERROR_EINVAL;
    *prio = thread->priority.load(std::memory_order_relaxed);
    return SCE_OK;
}

int APS5_VABI scePthreadGetthreadid(void) {
    return static_cast<int>(scePthreadSelf()->tid);
}

int APS5_VABI scePthreadRename(Pthread thread, const char* name) {
    if (!thread || !name) return SCE_KERNEL_ERROR_EINVAL;
    std::lock_guard lock(thread->nameLock);
    thread->name = name;
#ifdef _WIN32
    const std::wstring description(thread->name.begin(), thread->name.end());
    SetThreadDescription(static_cast<HANDLE>(thread->nativeHandle), description.c_str());
    ApplyJobAffinity(*thread, thread->nativeHandle);
#endif
    return SCE_OK;
}

int APS5_VABI scePthreadSetaffinity(Pthread thread, KernelCpumask mask) {
    if (!thread || mask == 0) return SCE_KERNEL_ERROR_EINVAL;
    thread->affinity.store(mask, std::memory_order_relaxed);
    return SCE_OK;
}

int APS5_VABI scePthreadSetcancelstate(int state, int* old_state) {
    if (state != GUEST_CANCEL_ENABLE && state != GUEST_CANCEL_DISABLE) return SCE_KERNEL_ERROR_EINVAL;
    if (old_state) *old_state = CurrentGuestState().cancelState;
    CurrentGuestState().cancelState = state;
    if (CurrentGuestState().cancelType == GUEST_CANCEL_ASYNCHRONOUS) ThreadCancel::Check();
    return SCE_OK;
}

int APS5_VABI scePthreadSetcanceltype(int type, int* old_type) {
    if (type != GUEST_CANCEL_DEFERRED && type != GUEST_CANCEL_ASYNCHRONOUS) return SCE_KERNEL_ERROR_EINVAL;
    if (old_type) *old_type = CurrentGuestState().cancelType;
    CurrentGuestState().cancelType = type;
    if (CurrentGuestState().cancelType == GUEST_CANCEL_ASYNCHRONOUS) ThreadCancel::Check();
    return SCE_OK;
}

void APS5_VABI scePthreadTestcancel() {
    ThreadCancel::Check();
}

int APS5_VABI scePthreadSetprio(Pthread thread, int prio) {
    if (!thread) return SCE_KERNEL_ERROR_EINVAL;
    thread->priority.store(prio, std::memory_order_relaxed);
    return SCE_OK;
}

int APS5_VABI scePthreadOnce(int32_t* once, void (APS5_VABI* init)(void)) {
    if (!once || !init) return SCE_KERNEL_ERROR_EINVAL;
    constexpr int32_t Never = 0;
    constexpr int32_t Done = 1;
    constexpr int32_t Running = 2;
    std::atomic_ref<int32_t> state(*once);
    int32_t expected = Never;
    if (state.compare_exchange_strong(expected, Running, std::memory_order_acq_rel)) {
        init();
        state.store(Done, std::memory_order_release);
        state.notify_all();
        return SCE_OK;
    }
    while ((expected = state.load(std::memory_order_acquire)) == Running) state.wait(Running, std::memory_order_acquire);
    return SCE_OK;
}

}

extern "C" {

void APS5_VABI __pthread_cxa_finalize_nid_postfix(void* argument) {
    CxaFinalize_nid_no_patch(argument);
}

}
