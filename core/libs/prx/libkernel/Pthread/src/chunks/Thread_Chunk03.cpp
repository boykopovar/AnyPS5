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

static void ReleaseThread(PthreadPrivate* thread) {
    if (thread->_adopted)
        return;
    if (thread->references.fetch_sub(1, std::memory_order_acq_rel) != 1)
        return;
    auto* expected = thread;
    if (finalThreadReleaseTestTarget.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel)) {
        if (const auto hook = finalThreadReleaseTestHook.exchange(nullptr, std::memory_order_acq_rel))
            hook(reinterpret_cast<Pthread>(thread));
    }
#ifdef _WIN32
    if (thread->wakeEvent)
        CloseHandle(static_cast<HANDLE>(thread->wakeEvent));
    if (!CloseHandle(thread->nativeHandle))
        throw std::system_error(GetLastError(), std::system_category(), "Closing guest thread handle");
#endif
    delete thread;
}

class GuestThreadReference {
public:
    GuestThreadReference() = default;
    bool Retain(PthreadPrivate* value) {
        auto references = value->references.load(std::memory_order_relaxed);
        while (references != 0) {
            if (value->references.compare_exchange_weak(references, references + 1, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                thread = value;
                return true;
            }
        }
        return false;
    }
    void Reset() {
        if (!thread) return;
        auto* retained = thread;
        thread = nullptr;
        ReleaseThread(retained);
    }
    ~GuestThreadReference() { Reset(); }
    GuestThreadReference(const GuestThreadReference&) = delete;
    GuestThreadReference& operator=(const GuestThreadReference&) = delete;

private:
    PthreadPrivate* thread = nullptr;
};

extern "C" void APS5_Test_SetFinalThreadReleaseHook(Pthread thread, FinalThreadReleaseTestHook hook) {
    finalThreadReleaseTestHook.store(hook, std::memory_order_relaxed);
    finalThreadReleaseTestTarget.store(static_cast<PthreadPrivate*>(thread), std::memory_order_release);
}

static std::mutex joinedLock;
static std::map<Pthread*, PthreadPrivate*> joinedThreads;

static void RetainJoinedThread(PthreadPrivate* thread) {
    std::lock_guard lock(joinedLock);
    Pthread* const storage = thread->_handleStorage;
    const auto found = joinedThreads.find(storage);
    if (found != joinedThreads.end() && found->second != thread) ReleaseThread(found->second);
    joinedThreads[storage] = thread;
}

static void RecycleJoinedThread(Pthread* storage) {
    std::lock_guard lock(joinedLock);
    const auto found = joinedThreads.find(storage);
    if (found == joinedThreads.end()) return;
    PthreadPrivate* joined = found->second;
    joinedThreads.erase(found);
    ReleaseThread(joined);
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
static std::wstring ThreadDescription(const std::string& name) {
    if (name.empty()) return {};
    const int length = MultiByteToWideChar(CP_UTF8, 0, name.data(), static_cast<int>(name.size()), nullptr, 0);
    std::wstring description(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, name.data(), static_cast<int>(name.size()), description.data(), length);
    return description;
}

static unsigned __stdcall StartNativeThread(void* opaque) {
    std::uint32_t incomingMxcsr = 0;
    asm volatile("stmxcsr %0" : "=m"(incomingMxcsr));
    std::unique_ptr<NativeThreadArgs> args(static_cast<NativeThreadArgs*>(opaque));
    auto* self = args->guest->self;
    self->hostMxcsr = incomingMxcsr;
    try {
        ULONG_PTR low = 0;
        ULONG_PTR high = 0;
        GetCurrentThreadStackLimits(&low, &high);
        if (high <= low || high - low < self->stackSize)
            throw std::runtime_error("Cannot query guest thread stack");
        self->stackAddress = reinterpret_cast<void*>(high - self->stackSize);
        if (!CommittedReadWrite(high - self->stackSize, high))
            throw std::runtime_error("Guest thread stack is not fully committed");
        self->threadId = std::this_thread::get_id();
        CurrentGuestState().current = self;
        if (!self->name.empty()) {
            SetThreadDescription(GetCurrentThread(), ThreadDescription(self->name).c_str());
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
    asm volatile("ldmxcsr %0" : : "m"(self->hostMxcsr));
    ReleaseThread(self);
    return 0;
}
#else
static void* StartNativeThread(void* opaque) {
    std::uint32_t incomingMxcsr = 0;
    asm volatile("stmxcsr %0" : "=m"(incomingMxcsr));
    std::unique_ptr<NativeThreadArgs> args(static_cast<NativeThreadArgs*>(opaque));
    auto* self = args->guest->self;
    self->hostMxcsr = incomingMxcsr;
    try {
        std::uintptr_t low = 0;
        std::uintptr_t high = 0;
        const auto reserve = HostStackReserve();
        if (!HostStackLimits(&low, &high) || high - low < reserve + self->stackSize + PS5_PAGE_SIZE - 1)
            throw std::runtime_error("Cannot query guest thread stack");
        self->stackAddress = reinterpret_cast<void*>((high - reserve - self->stackSize) & ~std::uintptr_t{PS5_PAGE_SIZE - 1});
        self->threadId = std::this_thread::get_id();
#if defined(__linux__)
        self->hostTid = static_cast<int>(gettid());
        RegisterGuestStackMapping(self);
#endif
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
    const auto stackEnd = reinterpret_cast<std::uintptr_t>(self->stackAddress) + self->stackSize;
    if (const auto frame = reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0)); frame > stackEnd)
        static_cast<volatile unsigned char*>(alloca(frame - stackEnd))[0] = 0;
    std::jmp_buf exitJump;
    if (_setjmp(exitJump) == 0) {
        threadExitJump = &exitJump;
        RunThread(std::unique_ptr<ThreadArgs>(guest));
    }
    threadExitJump = nullptr;
    asm volatile("ldmxcsr %0" : : "m"(self->hostMxcsr));
    return nullptr;
}
#endif


int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name) {
    if (!thread || !entry) throw std::runtime_error("scePthreadCreate: null arg");
    std::string nameCopy;
    if (name) {
        const int error = CopyGuestString_nid_no_patch(name, GUEST_THREAD_NAME_CAPACITY, nameCopy);
        if (error != 0) {
            *thread = nullptr;
            if (error == 63) return SCE_KERNEL_ERROR_ENAMETOOLONG;
            if (error == 12) return SCE_KERNEL_ERROR_ENOMEM;
            return SCE_KERNEL_ERROR_EFAULT;
        }
    }
    if (attr && !*attr) attr = nullptr;
    auto p = std::make_unique<PthreadPrivate>(attr ? *attr : nullptr);
    bool detached = false;
    if (attr && *attr) detached = ((*attr)->_detachstate == DETACH_DETACHED);
    p->_detached = detached;
    p->stackSize = attr ? (*attr)->_stacksize : GetDefaultStackSize_nid_no_patch();
    if (attr) {
        if ((*attr)->_inheritsched != GUEST_INHERIT_SCHED)
            p->scope = (*attr)->_scope;
        else if (CurrentGuestState().current)
            p->scope = CurrentGuestState().current->scope;
    } else if (CurrentGuestState().current) {
        p->scope = CurrentGuestState().current->scope;
    }
    if (name) p->name = std::move(nameCopy);
    p->createdSuspended = attr && (*attr)->_createSuspended;
    p->suspended.store(p->createdSuspended, std::memory_order_relaxed);
    p->pendingException.store(0, std::memory_order_relaxed);
    std::promise<bool> start;
    auto args = std::make_unique<ThreadArgs>(ThreadArgs{entry, arg, p.get()});
    sigprocmask_nid_postfix(1, nullptr, &args->guestSignalMask);
#if defined(_WIN32) || defined(__APPLE__)
    args->gsBase = GuestGsGet_nid_no_patch();
#endif
#ifdef _WIN32
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    if (p->stackSize < 16384 || p->stackSize > std::numeric_limits<unsigned>::max() - HOST_STACK_MARGIN - system.dwPageSize)
        throw std::runtime_error("scePthreadCreate: invalid Windows stack size");
    const std::size_t nativeStack = (p->stackSize + HOST_STACK_MARGIN + system.dwPageSize - 1) / system.dwPageSize * system.dwPageSize;
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
    published->_handleStorage = thread;
    *thread = published;
    start.set_value(true);
    RecycleJoinedThread(thread);
    if (detached)
        ReleaseThread(published);
#else
    const auto reserve = HostStackReserve();
    const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    if (p->stackSize < 16384 || p->stackSize > std::numeric_limits<std::size_t>::max() - reserve - HOST_STACK_MARGIN - page)
        throw std::runtime_error("scePthreadCreate: invalid stack size");
    const std::size_t nativeStack = (p->stackSize + reserve + HOST_STACK_MARGIN + page - 1) / page * page;
    sigset_t creatorMask;
    pthread_sigmask(SIG_SETMASK, nullptr, &creatorMask);
    args->signalMask = creatorMask;
#if defined(__linux__)
    ReapRetiredStacks();
    AllocateGuestStack(p.get(), nativeStack);
#endif
    auto native = std::make_unique<NativeThreadArgs>(NativeThreadArgs{std::move(args), start.get_future(), {}});
    auto initialized = native->initialized.get_future();
    pthread_attr_t nativeAttr;
    if (const int result = pthread_attr_init(&nativeAttr); result != 0) {
        FreeGuestStack(p.get());
        throw std::system_error(result, std::generic_category(), "Creating guest thread");
    }
#if defined(__linux__)
    int created = pthread_attr_setstack(&nativeAttr, static_cast<char*>(p->guestStack) + GUEST_STACK_GUARD, p->guestStackBytes - GUEST_STACK_GUARD);
#else
    int created = pthread_attr_setstacksize(&nativeAttr, nativeStack);
#endif
    if (created == 0) {
        sigset_t allSignals;
        sigfillset(&allSignals);
#ifdef __APPLE__
        KeepHostFaultSignalsUnblocked(allSignals);
#endif
        pthread_sigmask(SIG_BLOCK, &allSignals, nullptr);
        created = pthread_create(&p->hostThread, &nativeAttr, StartNativeThread, native.get());
        pthread_sigmask(SIG_SETMASK, &creatorMask, nullptr);
    }
    pthread_attr_destroy(&nativeAttr);
    if (created != 0) {
        FreeGuestStack(p.get());
        throw std::system_error(created, std::generic_category(), "Creating guest thread");
    }
    native.release();
    try {
        initialized.get();
        if (detached) {
            if (const int result = pthread_detach(p->hostThread); result != 0)
                throw std::system_error(result, std::generic_category(), "Detaching guest thread");
            RetireGuestStack(p.get());
        }
    } catch (...) {
        start.set_value(false);
        pthread_join(p->hostThread, nullptr);
        FreeGuestStack(p.get());
        throw;
    }
    auto* published = p.release();
    published->_handleStorage = thread;
    *thread = published;
    start.set_value(true);
    RecycleJoinedThread(thread);
    if (detached)
        ReleaseThread(published);
#endif
    return SCE_OK;
}

int APS5_VABI JoinThread_nid_no_patch(Pthread thread, void** retval, const KernelTimespec* abstime) {
    if (!thread) return SCE_KERNEL_ERROR_EINVAL;
    GuestThreadReference reference;
    auto registered = LockRegisteredGuestThread(thread);
    if (!registered) return SCE_KERNEL_ERROR_ESRCH;
    if (!reference.Retain(thread)) return SCE_KERNEL_ERROR_ESRCH;
    if (thread == CurrentGuestState().current) return SCE_KERNEL_ERROR_EDEADLK;
    if (thread->_detached) return SCE_KERNEL_ERROR_EINVAL;
    if (thread->_joined.load(std::memory_order_acquire)) return SCE_KERNEL_ERROR_ESRCH;
    registered.unlock();
    reference.Reset();
    ThreadCancel::Check();
    registered = LockRegisteredGuestThread(thread);
    if (!registered) return SCE_KERNEL_ERROR_ESRCH;
    if (!reference.Retain(thread)) return SCE_KERNEL_ERROR_ESRCH;
    {
        std::unique_lock lock(thread->_join_mtx);
        if (thread->_detached) return SCE_KERNEL_ERROR_EINVAL;
        if (thread->_joined.load(std::memory_order_acquire)) return SCE_KERNEL_ERROR_ESRCH;
        if (thread->_joining) return SCE_KERNEL_ERROR_EOPNOTSUPP;
        thread->_joining = true;
        registered.unlock();
        while (!thread->_finished.load()) {
            std::optional<std::uint64_t> deadline;
            if (abstime != nullptr) {
                KernelTimespec now{};
                if (clock_gettime_nid_postfix(0, &now) != 0) {
                    thread->_joining = false;
                    throw std::runtime_error("Joining guest thread: realtime clock unavailable");
                }
                if (abstime->tv_sec < now.tv_sec || (abstime->tv_sec == now.tv_sec && abstime->tv_nsec <= now.tv_nsec)) {
                    thread->_joining = false;
                    return SCE_KERNEL_ERROR_ETIMEDOUT;
                }
                const auto seconds = static_cast<std::uint64_t>(abstime->tv_sec) - static_cast<std::uint64_t>(now.tv_sec);
                const auto nanos = seconds > 1 ? std::int64_t{1000000000} : static_cast<std::int64_t>(seconds) * 1000000000 + abstime->tv_nsec - now.tv_nsec;
                deadline = TimedWait::DeadlineNanos(static_cast<std::uint64_t>((nanos + 999) / 1000));
            }
            ThreadCancel::Wait(thread->_join_cv, lock, deadline);
            if (ThreadCancel::Requested()) {
                thread->_joining = false;
                lock.unlock();
                ThreadCancel::Check();
                lock.lock();
                thread->_joining = true;
            }
        }
        thread->_joining = false;
        if (thread->_joined.exchange(true, std::memory_order_acq_rel)) return SCE_KERNEL_ERROR_ESRCH;
    }
#ifdef _WIN32
    if (WaitForSingleObject(thread->nativeHandle, INFINITE) != WAIT_OBJECT_0)
        throw std::system_error(GetLastError(), std::system_category(), "Joining guest thread");
#else
    if (const int result = pthread_join(thread->hostThread, nullptr); result != 0)
        throw std::system_error(result, std::generic_category(), "Joining guest thread");
    FreeGuestStack(thread);
#endif
    void* const returned = thread->_retval;
    const bool stored = !retval || GuestCopyOut_nid_no_patch(retval, &returned, sizeof(returned));
    RetainJoinedThread(thread);
    return stored ? SCE_OK : SCE_KERNEL_ERROR_EFAULT;
}

int APS5_VABI scePthreadJoin(Pthread thread, void** retval) {
    return JoinThread_nid_no_patch(thread, retval, nullptr);
}

int APS5_VABI scePthreadDetach(Pthread thread) {
    if (!thread) return SCE_KERNEL_ERROR_EINVAL;
    GuestThreadReference reference;
    auto registered = LockRegisteredGuestThread(thread);
    if (!registered) return SCE_KERNEL_ERROR_ESRCH;
    if (!reference.Retain(thread)) return SCE_KERNEL_ERROR_ESRCH;
    {
        std::lock_guard lock(thread->_join_mtx);
        if (thread->_joined.load(std::memory_order_acquire)) return SCE_KERNEL_ERROR_ESRCH;
        if (thread->_detached || thread->_joining) return SCE_KERNEL_ERROR_EINVAL;
        thread->_detached = true;
    }
    registered.unlock();
#ifdef _WIN32
    ReleaseThread(thread);
#else
    if (const int result = pthread_detach(thread->hostThread); result != 0)
        throw std::system_error(result, std::generic_category(), "Detaching guest thread");
    RetireGuestStack(thread);
    ReleaseThread(thread);
#endif
    return SCE_OK;
}

}
