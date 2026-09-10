#include "Pthread.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>
#include <limits>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#endif

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EINVAL = 0x80020016;
static constexpr int SCE_KERNEL_ERROR_ENOMEM = 0x8002000C;
static constexpr int SCE_KERNEL_ERROR_EBUSY = 0x80020010;
static constexpr int SCE_KERNEL_ERROR_EDEADLK = 0x80020023;
static constexpr int SCE_KERNEL_ERROR_EPERM = 0x80020001;
static constexpr int SCE_KERNEL_ERROR_ETIMEDOUT = 0x80020062;
static constexpr int SCE_KERNEL_ERROR_EAGAIN = 0x80020023;
static constexpr int SCE_KERNEL_ERROR_ESRCH = 0x80020003;
static constexpr int SCE_KERNEL_ERROR_ENOTSUP = 0x80020086;

static constexpr std::size_t DEFAULT_STACK_SIZE = 1u << 20;
static constexpr int DETACH_JOINABLE = 0;
static constexpr int DETACH_DETACHED = 1;
static constexpr int SCHED_FIFO_PS5 = 1;

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
    fprintf(stdout, "RunThread started\n");
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

int APS5_VABI scePthreadMutexattrInit(PthreadMutexattr* attr) {
    if (!attr) throw std::runtime_error("scePthreadMutexattrInit: null attr");
    auto* p = new (std::nothrow) PthreadMutexattrPrivate{MutexType::Normal};
    if (!p) return SCE_KERNEL_ERROR_ENOMEM;
    *attr = p;
    return SCE_OK;
}

int APS5_VABI scePthreadMutexattrDestroy(PthreadMutexattr* attr) {
    if (!attr || !*attr) throw std::runtime_error("scePthreadMutexattrDestroy: null attr");
    delete *attr;
    *attr = nullptr;
    return SCE_OK;
}

int APS5_VABI scePthreadMutexattrSettype(PthreadMutexattr* attr, int type) {
    if (!attr || !*attr) throw std::runtime_error("scePthreadMutexattrSettype: null attr");
    switch (type) {
    case 1: (*attr)->type = MutexType::ErrorCheck; break;
    case 2: (*attr)->type = MutexType::Recursive; break;
    case 3: (*attr)->type = MutexType::Normal; break;
    default: throw std::runtime_error("scePthreadMutexattrSettype: invalid type");
    }
    return SCE_OK;
}

int APS5_VABI scePthreadMutexInit(PthreadMutex* mutex, const PthreadMutexattr* attr, const char*) {
    if (!mutex) throw std::runtime_error("scePthreadMutexInit: null mutex");
    MutexType t = MutexType::Normal;
    if (attr && *attr) t = (*attr)->type;
    auto* p = new (std::nothrow) PthreadMutexPrivate();
    if (!p) return SCE_KERNEL_ERROR_ENOMEM;
    p->_type = t;
    *mutex = p;
    return SCE_OK;
}

int APS5_VABI scePthreadMutexDestroy(PthreadMutex* mutex) {
    if (!mutex || !*mutex) throw std::runtime_error("scePthreadMutexDestroy: null mutex");
    delete *mutex;
    *mutex = nullptr;
    return SCE_OK;
}

int APS5_VABI scePthreadMutexLock(PthreadMutex* mutex) {
    if (!mutex || !*mutex) throw std::runtime_error("scePthreadMutexLock: null mutex");
    auto* m = *mutex;
    const auto tid = std::this_thread::get_id();
    if (m->_type == MutexType::Recursive) {
        m->_rmtx.lock();
        m->_owner.store(tid, std::memory_order_relaxed);
        ++m->_count;
        return SCE_OK;
    }
    if (m->_type == MutexType::ErrorCheck) {
        if (m->_owner.load(std::memory_order_acquire) == tid) return SCE_KERNEL_ERROR_EDEADLK;
    }
    m->_mtx.lock();
    m->_owner.store(tid, std::memory_order_relaxed);
    return SCE_OK;
}

int APS5_VABI scePthreadMutexUnlock(PthreadMutex* mutex) {
    if (!mutex || !*mutex) throw std::runtime_error("scePthreadMutexUnlock: null mutex");
    auto* m = *mutex;
    if (m->_type == MutexType::ErrorCheck || m->_type == MutexType::Normal) {
        if (m->_owner.load(std::memory_order_acquire) != std::this_thread::get_id())
            return SCE_KERNEL_ERROR_EPERM;
    }
    if (m->_type == MutexType::Recursive) {
        if (--m->_count == 0) m->_owner.store(std::thread::id{}, std::memory_order_relaxed);
        m->_rmtx.unlock();
        return SCE_OK;
    }
    m->_owner.store(std::thread::id{}, std::memory_order_relaxed);
    m->_mtx.unlock();
    return SCE_OK;
}

int APS5_VABI scePthreadCondattrInit(PthreadCondattr* attr) {
    if (!attr) throw std::runtime_error("scePthreadCondattrInit: null attr");
    auto* p = new (std::nothrow) PthreadCondattrPrivate{0};
    if (!p) return SCE_KERNEL_ERROR_ENOMEM;
    *attr = p;
    return SCE_OK;
}

int APS5_VABI scePthreadCondattrDestroy(PthreadCondattr* attr) {
    if (!attr || !*attr) throw std::runtime_error("scePthreadCondattrDestroy: null attr");
    delete *attr;
    *attr = nullptr;
    return SCE_OK;
}

int APS5_VABI scePthreadCondInit(PthreadCond* cond, const PthreadCondattr*, const char*) {
    if (!cond) throw std::runtime_error("scePthreadCondInit: null cond");
    auto* p = new (std::nothrow) PthreadCondPrivate{};
    if (!p) return SCE_KERNEL_ERROR_ENOMEM;
    *cond = p;
    return SCE_OK;
}

int APS5_VABI scePthreadCondDestroy(PthreadCond* cond) {
    if (!cond || !*cond) throw std::runtime_error("scePthreadCondDestroy: null cond");
    delete *cond;
    *cond = nullptr;
    return SCE_OK;
}

int APS5_VABI scePthreadCondSignal(PthreadCond* cond) {
    if (!cond || !*cond) throw std::runtime_error("scePthreadCondSignal: null cond");
    (*cond)->_cv.notify_one();
    return SCE_OK;
}

int APS5_VABI scePthreadCondBroadcast(PthreadCond* cond) {
    if (!cond || !*cond) throw std::runtime_error("scePthreadCondBroadcast: null cond");
    (*cond)->_cv.notify_all();
    return SCE_OK;
}

int APS5_VABI scePthreadCondTimedwait(PthreadCond* cond, PthreadMutex* mutex, unsigned int usec) {
    if (!cond || !*cond || !mutex || !*mutex)
        throw std::runtime_error("scePthreadCondTimedwait: null arg");
    auto* m = *mutex;
    auto* c = *cond;
    if (m->_type == MutexType::Recursive) {
        std::unique_lock<std::recursive_timed_mutex> lk(m->_rmtx, std::adopt_lock);
        auto res = c->_cv.wait_for(lk, std::chrono::microseconds(usec));
        lk.release();
        return res == std::cv_status::timeout ? SCE_KERNEL_ERROR_ETIMEDOUT : SCE_OK;
    }
    std::unique_lock<std::timed_mutex> lk(m->_mtx, std::adopt_lock);
    auto res = c->_cv.wait_for(lk, std::chrono::microseconds(usec));
    lk.release();
    return res == std::cv_status::timeout ? SCE_KERNEL_ERROR_ETIMEDOUT : SCE_OK;
}

int APS5_VABI scePthreadAttrInit(PthreadAttr* attr) {
    if (!attr) throw std::runtime_error("scePthreadAttrInit: null attr");
    auto* p = new (std::nothrow) PthreadAttrPrivate{};
    if (!p) return SCE_KERNEL_ERROR_ENOMEM;
    p->_stacksize = DEFAULT_STACK_SIZE;
    p->_detachstate = DETACH_JOINABLE;
    p->_schedpriority = 700;
    p->_schedpolicy = SCHED_FIFO_PS5;
    p->_inheritsched = 4;
    *attr = p;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr) {
    if (!attr || !*attr) throw std::runtime_error("scePthreadAttrDestroy: null attr");
    delete *attr;
    *attr = nullptr;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetdetachstate(PthreadAttr* attr, int detachstate) {
    if (!attr || !*attr) throw std::runtime_error("scePthreadAttrSetdetachstate: null attr");
    if (detachstate != DETACH_JOINABLE && detachstate != DETACH_DETACHED)
        throw std::runtime_error("scePthreadAttrSetdetachstate: invalid state");
    (*attr)->_detachstate = detachstate;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetschedparam(PthreadAttr* attr, const KernelSchedParam* param) {
    if (!attr || !*attr || !param)
        throw std::runtime_error("scePthreadAttrSetschedparam: null arg");
    (*attr)->_schedpriority = param->sched_priority;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrSetstacksize(PthreadAttr* attr, std::size_t stacksize) {
    if (!attr || !*attr) throw std::runtime_error("scePthreadAttrSetstacksize: null attr");
    if (stacksize < 16384) throw std::runtime_error("scePthreadAttrSetstacksize: too small");
#ifdef _WIN32
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    if (stacksize % system.dwPageSize != 0 || stacksize > std::numeric_limits<unsigned>::max())
        throw std::runtime_error("scePthreadAttrSetstacksize: invalid Windows stack size");
#endif
    (*attr)->_stacksize = stacksize;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGetstack(const PthreadAttr* attr, void** stackaddr, std::size_t* stacksize) {
    if (!attr || !*attr || !stackaddr || !stacksize)
        throw std::runtime_error("scePthreadAttrGetstack: null arg");
    *stackaddr = (*attr)->stackAddress;
    *stacksize = (*attr)->_stacksize;
    return SCE_OK;
}

int APS5_VABI scePthreadAttrGet(Pthread thread, PthreadAttr* attr) {
    if (!thread || !attr || !*attr)
        throw std::runtime_error("scePthreadAttrGet: null arg");
    (*attr)->_stacksize = thread->stackSize;
    (*attr)->stackAddress = thread->stackAddress;
    (*attr)->_detachstate = thread->_detached ? DETACH_DETACHED : DETACH_JOINABLE;
    (*attr)->_schedpriority = 700;
    (*attr)->_schedpolicy = SCHED_FIFO_PS5;
    (*attr)->_inheritsched = 4;
    return SCE_OK;
}

int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char*) {
    if (!thread || !entry) throw std::runtime_error("scePthreadCreate: null arg");
    if (attr && !*attr) throw std::runtime_error("scePthreadCreate: null attributes");
    auto p = std::make_unique<PthreadPrivate>();
    bool detached = false;
    if (attr && *attr) detached = ((*attr)->_detachstate == DETACH_DETACHED);
    p->_detached = detached;
    p->stackSize = attr ? (*attr)->_stacksize : DEFAULT_STACK_SIZE;
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
    return currentThread;
#else
    return nullptr;
#endif
}

void APS5_VABI scePthreadYield() {
    std::this_thread::yield();
}

} // extern "C"
