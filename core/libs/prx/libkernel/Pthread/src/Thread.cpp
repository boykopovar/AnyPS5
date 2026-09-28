#include "../include/Pthread.hpp"
#include "../include/ThreadLifecycle.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/CpuTopology.hpp"
#include <algorithm>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <functional>
#include <string>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <system_error>

#ifndef _WIN32
#include <csetjmp>
#include <pthread.h>
#endif

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EINVAL = 0x80020016;

static constexpr std::size_t DEFAULT_STACK_SIZE = 1u << 20;
static constexpr int DETACH_DETACHED = 1;
static thread_local int guestCancelState = 0; // PTHREAD_CANCEL_ENABLE
static thread_local int guestCancelType = 0;  // PTHREAD_CANCEL_DEFERRED
static constexpr std::size_t THREAD_NAME_CAPACITY = 32;

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#include <limits>
#endif

struct ThreadArgs {
    PthreadEntry entry;
    void* arg;
    PthreadPrivate* self;
};

static std::atomic<thread_dtors_func_t> threadDtors{nullptr};
static std::atomic<get_thread_atexit_count_func_t> threadAtexitCount{nullptr};
static std::atomic<thread_atexit_report_func_t> threadAtexitReport{nullptr};
static thread_local PthreadPrivate* currentThread = nullptr;
static thread_local bool threadFinishing = false;
#ifndef _WIN32
static thread_local std::unique_ptr<PthreadPrivate> adoptedThread;
// pthread_exit force-unwinds through guest frames, whose personality resolves to
// libc.prx's __gxx_personality_v0; that routine cannot read libgcc's unwind context
// and aborts. scePthreadExit instead jumps back to the thread start routine, which,
// like _endthreadex on Windows, skips the guest frames without unwinding them.
static thread_local std::jmp_buf* threadExitJump = nullptr;
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
    if (!self || self != currentThread)
        throw std::runtime_error("Finishing an unregistered guest thread");
    if (threadFinishing)
        throw std::runtime_error("Guest thread is already finishing");
    threadFinishing = true;
    if (const auto callback = threadDtors.load())
        callback();
    {
        std::unique_lock<std::mutex> lk(self->_join_mtx);
        self->_retval = retval;
        self->_finished.store(true, std::memory_order_release);
    }
    self->_join_cv.notify_all();
}

static void RunThread(std::unique_ptr<ThreadArgs> args) {
    APS5_LOG_OUT("RunThread entry=0x%llx arg=%p self=%p", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(args->entry)), args->arg, static_cast<void*>(args->self));
    const auto entry = args->entry;
    void* arg = args->arg;
    PthreadPrivate* self = args->self;
    currentThread = self;
    args.reset();
    finishThread(self, entry(arg));
    currentThread = nullptr;
}

static void ReleaseThread(PthreadPrivate* thread) {
    if (thread->_adopted)
        return;
    if (thread->references.fetch_sub(1, std::memory_order_acq_rel) != 1)
        return;
#ifdef _WIN32
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

struct NativeThreadArgs {
    std::unique_ptr<ThreadArgs> guest;
    std::future<bool> start;
    std::promise<void> initialized;
};

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
        currentThread = self;
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
    currentThread = nullptr;
    ReleaseThread(self);
    return 0;
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
    if (p->stackSize < 16384 || p->stackSize % system.dwPageSize != 0 || p->stackSize > std::numeric_limits<unsigned>::max())
        throw std::runtime_error("scePthreadCreate: invalid Windows stack size");
    auto native = std::make_unique<NativeThreadArgs>(NativeThreadArgs{std::move(args), start.get_future(), {}});
    auto initialized = native->initialized.get_future();
    const auto handle = _beginthreadex(nullptr, static_cast<unsigned>(p->stackSize), StartNativeThread, native.get(), 0, nullptr);
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
    auto* self = p.get();
    p->_thr = std::thread([self, args = std::move(args), ready = start.get_future()]() mutable {
        if (!ready.get()) return;
        self->threadId = std::this_thread::get_id();
        struct ThreadGuard {
            PthreadPrivate* self;
            ~ThreadGuard() {
                currentThread = nullptr;
                ReleaseThread(self);
            }
        } guard{self};
        std::jmp_buf exitJump;
        if (setjmp(exitJump) == 0) {
            threadExitJump = &exitJump;
            RunThread(std::move(args));
        }
        threadExitJump = nullptr;
    });
    try {
        if (detached) p->_thr.detach();
    } catch (...) {
        start.set_value(false);
        p->_thr.join();
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
    if (thread == currentThread)
        throw std::runtime_error("scePthreadJoin: cannot join current thread");
#ifdef _WIN32
    if (WaitForSingleObject(thread->nativeHandle, INFINITE) != WAIT_OBJECT_0)
        throw std::system_error(GetLastError(), std::system_category(), "Joining guest thread");
    if (retval) *retval = thread->_retval;
    ReleaseThread(thread);
#else
    if (thread->_thr.joinable()) thread->_thr.join();
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
    if (thread->_thr.joinable()) thread->_thr.detach();
    ReleaseThread(thread);
#endif
    return SCE_OK;
}

void APS5_VABI scePthreadExit(void* retval) {
    if (!currentThread)
        throw std::runtime_error("scePthreadExit: current thread is not registered");
    auto* self = currentThread;
    finishThread(self, retval);
    currentThread = nullptr;
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
    if (!currentThread) {
        auto adopted = std::make_unique<PthreadPrivate>();
        HANDLE handle = nullptr;
        if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &handle, 0, FALSE, DUPLICATE_SAME_ACCESS))
            throw std::system_error(GetLastError(), std::system_category(), "Adopting guest thread");
        adopted->nativeHandle = handle;
        adopted->threadId = std::this_thread::get_id();
        adopted->_detached = true;
        adopted->references.store(1, std::memory_order_relaxed);
        currentThread = adopted.release();
    }
#else
    if (!currentThread) {
        adoptedThread = std::make_unique<PthreadPrivate>();
        adoptedThread->_detached = true;
        adoptedThread->_adopted = true;
        adoptedThread->threadId = std::this_thread::get_id();
        currentThread = adoptedThread.get();
    }
#endif
    return currentThread;
}

void APS5_VABI scePthreadYield() {
    std::this_thread::yield();
}

int APS5_VABI scePthreadCancel(Pthread thread) {
 (void)thread;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI scePthreadEqual(Pthread thread1, Pthread thread2) {
    return thread1 == thread2 ? 1 : 0;
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
#ifdef _WIN32
    return static_cast<int>(GetCurrentThreadId());
#else
    return static_cast<int>(std::hash<std::thread::id>{}(std::this_thread::get_id()) & 0x7fffffff);
#endif
}

int APS5_VABI scePthreadRename(Pthread thread, const char* name) {
    if (name == nullptr || name[0] == '\0') return SCE_KERNEL_ERROR_EINVAL;
    auto* target = thread ? thread : currentThread;
#ifdef _WIN32
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, nullptr, 0);
    if (size == 0) return SCE_KERNEL_ERROR_EINVAL;
    std::wstring wide(static_cast<std::size_t>(size), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, wide.data(), size) == 0)
        return SCE_KERNEL_ERROR_EINVAL;
    const HANDLE handle = thread == nullptr ? GetCurrentThread() : thread->nativeHandle;
    if (FAILED(SetThreadDescription(handle, wide.c_str()))) return SCE_KERNEL_ERROR_EINVAL;
    if (target) {
        std::lock_guard lock(target->nameLock);
        target->name = name;
    }
    if (thread) ApplyJobAffinity(*thread, thread->nativeHandle);
    return SCE_OK;
#else
    // Linux limits native names to 15 bytes; truncate at that boundary.
    const std::string nativeName(name, std::char_traits<char>::length(name));
    if (pthread_setname_np(thread == nullptr ? pthread_self() : thread->_thr.native_handle(),
                           nativeName.substr(0, 15).c_str()) != 0)
        return SCE_KERNEL_ERROR_EINVAL;
    if (target) {
        std::lock_guard lock(target->nameLock);
        target->name = name;
    }
    return SCE_OK;
#endif
}

int APS5_VABI scePthreadSetaffinity(Pthread thread, KernelCpumask mask) {
    if (!thread || mask == 0) return SCE_KERNEL_ERROR_EINVAL;
    thread->affinity.store(mask, std::memory_order_relaxed);
    return SCE_OK;
}

int APS5_VABI scePthreadSetcancelstate(int state, int* old_state) {
    if (state != 0 && state != 1) return SCE_KERNEL_ERROR_EINVAL;
    if (old_state != nullptr) *old_state = guestCancelState;
    guestCancelState = state;
    return SCE_OK;
}

int APS5_VABI scePthreadSetcanceltype(int type, int* old_type) {
    if (type != 0 && type != 2) return SCE_KERNEL_ERROR_EINVAL;
    if (old_type != nullptr) *old_type = guestCancelType;
    guestCancelType = type;
    return SCE_OK;
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
    (void)argument;
    NotImplemented_nid_no_patch(__func__);
}

}
