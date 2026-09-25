#include "../include/Pthread.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <system_error>

#ifndef _WIN32
#include <pthread.h>
#endif

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EINVAL = 0x80020016;

static constexpr std::size_t DEFAULT_STACK_SIZE = 1u << 20;
static constexpr int DETACH_DETACHED = 1;
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

static void FinishThread(PthreadPrivate* self, void* retval) {
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
    args.reset();
    FinishThread(self, entry(arg));
}

#ifdef _WIN32
static thread_local PthreadPrivate* currentThread = nullptr;

static void ReleaseThread(PthreadPrivate* thread) {
    if (thread->references.fetch_sub(1, std::memory_order_acq_rel) != 1)
        return;
    if (!CloseHandle(thread->nativeHandle))
        throw std::system_error(GetLastError(), std::system_category(), "Closing guest thread handle");
    delete thread;
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
    p->_thr = std::thread([args = std::move(args), ready = start.get_future()]() mutable {
        if (ready.get()) RunThread(std::move(args));
    });
    try {
        if (detached) p->_thr.detach();
    } catch (...) {
        start.set_value(false);
        p->_thr.join();
        throw;
    }
    *thread = p.release();
    start.set_value(true);
#endif
    return SCE_OK;
}

int APS5_VABI scePthreadJoin(Pthread thread, void** retval) {
    if (!thread) throw std::runtime_error("scePthreadJoin: null thread");
    if (thread->_detached) return SCE_KERNEL_ERROR_EINVAL;
#ifdef _WIN32
    if (thread == currentThread)
        throw std::runtime_error("scePthreadJoin: cannot join current thread");
    if (WaitForSingleObject(thread->nativeHandle, INFINITE) != WAIT_OBJECT_0)
        throw std::system_error(GetLastError(), std::system_category(), "Joining guest thread");
    if (retval) *retval = thread->_retval;
    ReleaseThread(thread);
#else
    if (thread->_thr.joinable()) thread->_thr.join();
    if (retval) *retval = thread->_retval;
    delete thread;
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
#endif
    return SCE_OK;
}

void APS5_VABI scePthreadExit(void* retval) {
#ifdef _WIN32
    if (!currentThread)
        throw std::runtime_error("scePthreadExit: current thread is not registered");
    auto* self = currentThread;
    FinishThread(self, retval);
    currentThread = nullptr;
    ReleaseThread(self);
    _endthreadex(0);
#else
    pthread_exit(retval);
#endif
    __builtin_unreachable();
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
    return currentThread;
#else
    return nullptr;
#endif
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
    if (!thread || !name) return SCE_KERNEL_ERROR_EINVAL;
    std::lock_guard lock(thread->nameLock);
    thread->name = name;
#ifdef _WIN32
    const std::wstring description(thread->name.begin(), thread->name.end());
    SetThreadDescription(static_cast<HANDLE>(thread->nativeHandle), description.c_str());
#endif
    return SCE_OK;
}

int APS5_VABI scePthreadSetaffinity(Pthread thread, KernelCpumask mask) {
    if (!thread || mask == 0) return SCE_KERNEL_ERROR_EINVAL;
    thread->affinity.store(mask, std::memory_order_relaxed);
    return SCE_OK;
}

int APS5_VABI scePthreadSetcancelstate(int state, int* old_state) {
    static thread_local int cancelState = 0;
    if (old_state) *old_state = cancelState;
    cancelState = state;
    return SCE_OK;
}

int APS5_VABI scePthreadSetcanceltype(int type, int* old_type) {
    static thread_local int cancelType = 0;
    if (old_type) *old_type = cancelType;
    cancelType = type;
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
