#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "../include/ThreadLifecycle.hpp"
#include "../include/Cancel.hpp"
#include "prx/libc/include/General.hpp"
#include "../include/Pthread.hpp"
#include "Common.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <atomic>
#include <mutex>
#include <cstring>
#include <thread>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadDetach(Pthread thread);
void APS5_VABI scePthreadExit(void* retval);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI JoinThread_nid_no_patch(Pthread thread, void** retval, const KernelTimespec* abstime);
int APS5_VABI scePthreadRename(Pthread thread, const char* name);
int APS5_VABI scePthreadGetname(Pthread thread, char* name);
Pthread APS5_VABI scePthreadSelf();
int APS5_VABI scePthreadEqual(Pthread thread1, Pthread thread2);
int APS5_VABI scePthreadOnce(int32_t* once, void (APS5_VABI* init)(void));
int APS5_VABI scePthreadKeyCreate(PthreadKey* key, pthread_key_destructor_func_t destructor);
int APS5_VABI scePthreadKeyDelete(PthreadKey key);
void* APS5_VABI scePthreadGetspecific(PthreadKey key);
int APS5_VABI scePthreadSetspecific(PthreadKey key, void* value);
int APS5_VABI scePthreadMutexattrInit(PthreadMutexattr* attr);
int APS5_VABI scePthreadMutexattrSettype(PthreadMutexattr* attr, int type);
int APS5_VABI scePthreadMutexattrDestroy(PthreadMutexattr* attr);
int APS5_VABI scePthreadMutexInit(PthreadMutex* mutex, const PthreadMutexattr* attr, const char* name);
int APS5_VABI scePthreadMutexDestroy(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexLock(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexTrylock(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexUnlock(PthreadMutex* mutex);
int APS5_VABI scePthreadCondInit(PthreadCond* cond, const PthreadCondattr* attr, const char* name);
int APS5_VABI scePthreadCondDestroy(PthreadCond* cond);
int APS5_VABI scePthreadCondWait(PthreadCond* cond, PthreadMutex* mutex);
int APS5_VABI scePthreadCondSignal(PthreadCond* cond);
int APS5_VABI scePthreadCondBroadcast(PthreadCond* cond);
int APS5_VABI scePthreadCancel(Pthread thread);
int APS5_VABI scePthreadSetcancelstate(int state, int* old_state);
int APS5_VABI scePthreadSetcanceltype(int type, int* old_type);
void APS5_VABI scePthreadTestcancel();
int APS5_VABI scePthreadSetprio(Pthread thread, int prio);
int APS5_VABI scePthreadGetprio(Pthread thread, int* prio);
void APS5_VABI scePthreadYield();
}

static constexpr int GUEST_SCHED_FIFO = 1;
static constexpr int GUEST_SCHED_RR = 3;

namespace {
std::atomic<int> guestConcurrency{0};
}

static bool AcceptsSchedParam(int policy, int priority) {
    if (policy != GUEST_SCHED_FIFO && policy != GUEST_SCHED_RR) return false;
    return (priority >= 256 && priority <= 767) || (priority >= 896 && priority <= 959);
}
static constexpr std::size_t GUEST_CPUSET_BYTES = 32;
static constexpr int GUEST_ERANGE = 34;

extern "C" {
int* APS5_VABI __error_nid_postfix();
int APS5_VABI scePthreadGetaffinity(Pthread thread, KernelCpumask* mask);
int APS5_VABI scePthreadSetaffinity(Pthread thread, KernelCpumask mask);
KernelCpumask APS5_VABI sceKernelGetAvailableCpumask(void);
int APS5_VABI getpid_nid_postfix(void);

int APS5_VABI cpuset_getaffinity_nid_postfix(int level, int which, std::int64_t id, std::size_t size, void* mask) {
    constexpr int LevelRoot = 1, LevelCpuset = 2, LevelWhich = 3;
    constexpr int WhichThread = 1, WhichProcess = 2, WhichCpuset = 3, WhichIrq = 4, WhichJail = 5, WhichDomain = 6;
    constexpr int GuestEsrch = 3, GuestEfault = 14, GuestEinval = 22, GuestErange = 34;
    constexpr std::size_t MaximumSize = 256 / 8;
    const auto fail = [](int error) {
        *__error_nid_postfix() = error;
        return -1;
    };
    if (size < sizeof(KernelCpumask) || size > MaximumSize) return fail(GuestErange);
    if (!mask) return fail(GuestEfault);
    auto* self = scePthreadSelf();
    KernelCpumask affinity = 0;
    switch (which) {
    case WhichThread: {
        const auto tid = id == -1 ? self->tid : id;
        if (!GetLiveGuestThreadAffinity_nid_no_patch(tid, affinity)) return fail(GuestEsrch);
        break;
    }
    case WhichProcess:
        if (id != -1 && id != getpid_nid_postfix()) return fail(GuestEsrch);
        break;
    case WhichCpuset:
        if (id != -1) NotImplemented_nid_no_patch("cpuset_getaffinity of a numbered cpuset");
        break;
    case WhichJail:
        NotImplemented_nid_no_patch("cpuset_getaffinity of a jail's cpuset");
        break;
    case WhichIrq:
    case WhichDomain:
        break;
    default:
        return fail(GuestEinval);
    }
    switch (level) {
    case LevelRoot:
    case LevelCpuset:
        if (which == WhichIrq || which == WhichDomain) return fail(GuestEinval);
        if (level == LevelRoot) NotImplemented_nid_no_patch("cpuset_getaffinity at CPU_LEVEL_ROOT");
        affinity = sceKernelGetAvailableCpumask();
        break;
    case LevelWhich:
        if (which == WhichIrq) NotImplemented_nid_no_patch("cpuset_getaffinity of an interrupt");
        if (which == WhichDomain) NotImplemented_nid_no_patch("cpuset_getaffinity of a memory domain");
        if (which == WhichProcess) affinity = LiveGuestThreadsAffinity();
        else if (which == WhichCpuset) affinity = sceKernelGetAvailableCpumask();
        break;
    default:
        return fail(GuestEinval);
    }
    std::memset(mask, 0, size);
    std::memcpy(mask, &affinity, sizeof(affinity));
    return 0;
}

int APS5_VABI cpuset_setaffinity_nid_postfix(int level, int which, std::int64_t id, std::size_t size, const void* mask) {
    constexpr int LevelRoot = 1, LevelCpuset = 2, LevelWhich = 3;
    constexpr int WhichThread = 1, WhichProcess = 2, WhichCpuset = 3, WhichIrq = 4, WhichJail = 5, WhichDomain = 6;
    constexpr int WhichInterruptHandler = 7, WhichInterruptThread = 8;
    constexpr int GuestEsrch = 3, GuestEdeadlk = 11, GuestEfault = 14, GuestEinval = 22, GuestErange = 34;
    constexpr std::size_t MaximumSize = 256 / 8;
    const auto fail = [](int error) {
        *__error_nid_postfix() = error;
        return -1;
    };
    if (size < sizeof(KernelCpumask) || size > MaximumSize) return fail(GuestErange);
    if (!mask) return fail(GuestEfault);
    const auto* bytes = static_cast<const std::uint8_t*>(mask);
    for (std::size_t index = sizeof(KernelCpumask); index < size; ++index) {
        if (bytes[index] != 0) return fail(GuestEinval);
    }
    auto* self = scePthreadSelf();
    const auto threadExists = [&] { return id == -1 || id == self->tid || LiveGuestThreadExists(id); };
    switch (level) {
    case LevelRoot:
    case LevelCpuset:
        switch (which) {
        case WhichThread:
            if (!threadExists()) return fail(GuestEsrch);
            break;
        case WhichProcess:
            if (id != -1 && id != getpid_nid_postfix()) return fail(GuestEsrch);
            break;
        case WhichCpuset:
            if (id != -1) NotImplemented_nid_no_patch("cpuset_setaffinity of a numbered cpuset");
            break;
        case WhichJail:
            NotImplemented_nid_no_patch("cpuset_setaffinity of a jail's cpuset");
            break;
        case WhichIrq:
        case WhichDomain:
        default:
            return fail(GuestEinval);
        }
        NotImplemented_nid_no_patch(level == LevelRoot ? "cpuset_setaffinity at CPU_LEVEL_ROOT" : "cpuset_setaffinity at CPU_LEVEL_CPUSET");
        return fail(GuestEinval);
    case LevelWhich:
        break;
    default:
        return fail(GuestEinval);
    }
    switch (which) {
    case WhichThread:
        if (!threadExists()) return fail(GuestEsrch);
        break;
    case WhichProcess:
        if (id != -1 && id != getpid_nid_postfix()) return fail(GuestEsrch);
        break;
    case WhichCpuset:
        NotImplemented_nid_no_patch(id == -1 ? "cpuset_setaffinity of the calling thread's base cpuset" : "cpuset_setaffinity of a numbered cpuset");
        break;
    case WhichJail:
        NotImplemented_nid_no_patch("cpuset_setaffinity of a jail's cpuset");
        break;
    case WhichIrq:
    case WhichInterruptHandler:
    case WhichInterruptThread:
        NotImplemented_nid_no_patch("cpuset_setaffinity of an interrupt");
        break;
    default:
        return fail(GuestEinval);
    }
    KernelCpumask requested = 0;
    std::memcpy(&requested, bytes, sizeof(requested));
    const KernelCpumask affinity = requested & sceKernelGetAvailableCpumask();
    if (affinity == 0) return fail(GuestEdeadlk);
    if (which == WhichThread) {
        const auto tid = id == -1 ? self->tid : id;
        if (!SetLiveGuestThreadAffinity_nid_no_patch(tid, affinity)) return fail(GuestEsrch);
    } else SetLiveGuestThreadsAffinity(affinity);
    return 0;
}

struct GuestRtPrio {
    std::uint16_t type;
    std::uint16_t prio;
};

int APS5_VABI rtprio_thread_nid_postfix(int function, int lwpid, GuestRtPrio* priority) {
    constexpr int Lookup = 0, Set = 1;
    constexpr int GuestEsrch = 3, GuestEfault = 14, GuestEinval = 22;
    constexpr std::uint16_t Normal = 3;
    const auto fail = [](int error) {
        *__error_nid_postfix() = error;
        return -1;
    };
    if (function != Lookup && function != Set) return fail(GuestEinval);
    if (priority == nullptr) return fail(GuestEfault);
    auto* self = scePthreadSelf();
    if (lwpid != 0 && static_cast<std::int64_t>(lwpid) != static_cast<std::int64_t>(self->tid)) {
        if (!LiveGuestThreadExists(lwpid)) return fail(GuestEsrch);
        NotImplemented_nid_no_patch("rtprio_thread: a thread other than the calling one");
    }
    const int policy = self->schedPolicy.load(std::memory_order_relaxed);
    if (policy == 1 || policy == 3) NotImplemented_nid_no_patch("rtprio_thread: a guest thread's real-time class");
    if (function == Lookup) {
        priority->type = Normal;
        priority->prio = 0;
        return 0;
    }
    if (priority->type == Normal && priority->prio == 0) return 0;
    NotImplemented_nid_no_patch("rtprio_thread: a real-time priority change");
}

int APS5_VABI pthread_setconcurrency_nid_postfix(int newLevel) {
    constexpr int GuestEinval = 22;
    if (newLevel < 0) return GuestEinval;
    guestConcurrency.store(newLevel, std::memory_order_relaxed);
    return 0;
}

int APS5_VABI pthread_getconcurrency_nid_postfix() {
    return guestConcurrency.load(std::memory_order_relaxed);
}

int APS5_VABI pthread_create_nid_postfix(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg) {
    return PosixThread::ToErrno(scePthreadCreate(thread, attr, entry, arg, nullptr));
}

int APS5_VABI pthread_create_name_np_nid_postfix(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name) {
    return PosixThread::ToErrno(scePthreadCreate(thread, attr, entry, arg, name));
}

int APS5_VABI pthread_detach_nid_postfix(Pthread thread) {
    return PosixThread::ToErrno(scePthreadDetach(thread));
}

void APS5_VABI pthread_exit_nid_postfix(void* value) {
    scePthreadExit(value);
}

int APS5_VABI scePthreadGetschedparam(Pthread thread, int* policy, KernelSchedParam* param) {
    if (!policy || !param) return SCE_KERNEL_ERROR_EINVAL;
    const int result = scePthreadGetprio(thread, &param->sched_priority);
    if (result == 0) *policy = thread->schedPolicy.load(std::memory_order_relaxed);
    return result;
}

int APS5_VABI scePthreadSetschedparam(Pthread thread, int policy, const KernelSchedParam* param) {
    if (!param || !AcceptsSchedParam(policy, param->sched_priority)) return SCE_KERNEL_ERROR_EINVAL;
    const int result = scePthreadSetprio(thread, param->sched_priority);
    if (result == 0) thread->schedPolicy.store(policy, std::memory_order_relaxed);
    return result;
}

int APS5_VABI pthread_getschedparam_nid_postfix(Pthread thread, int* policy, KernelSchedParam* param) {
    if (!policy || !param) return PosixThread::GUEST_EINVAL;
    return PosixThread::ToErrno(scePthreadGetschedparam(thread, policy, param));
}

int APS5_VABI pthread_getprio_nid_postfix(Pthread thread) {
    const auto lock = LockLiveGuestThread(thread);
    if (!lock.owns_lock()) {
        *__error_nid_postfix() = PosixThread::GUEST_ESRCH;
        return -1;
    }
    return thread->priority.load(std::memory_order_relaxed);
}

int APS5_VABI pthread_getaffinity_np_nid_postfix(Pthread thread, std::size_t cpusetSize, void* cpuset) {
    if (!thread) return PosixThread::GUEST_EINVAL;
    if (cpusetSize != GUEST_CPUSET_BYTES) return GUEST_ERANGE;
    if (!cpuset) return PosixThread::GUEST_EFAULT;
    KernelCpumask mask = 0;
    const int result = scePthreadGetaffinity(thread, &mask);
    if (result != 0) return PosixThread::ToErrno(result);
    std::memset(cpuset, 0, GUEST_CPUSET_BYTES);
    std::memcpy(cpuset, &mask, sizeof(mask));
    return 0;
}

int APS5_VABI pthread_setaffinity_np_nid_postfix(Pthread thread, std::size_t cpusetSize, const void* cpuset) {
    constexpr int GuestEdeadlk = 11;
    if (thread != scePthreadSelf()) {
        if (!thread) return PosixThread::GUEST_EINVAL;
        if (!GuestThreadIsLive(thread)) return PosixThread::GUEST_ESRCH;
    }
    if (cpusetSize != GUEST_CPUSET_BYTES) return GUEST_ERANGE;
    if (!cpuset) return PosixThread::GUEST_EFAULT;
    KernelCpumask requested = 0;
    std::memcpy(&requested, cpuset, sizeof(requested));
    const KernelCpumask affinity = requested & sceKernelGetAvailableCpumask();
    if (affinity == 0) return GuestEdeadlk;
    return PosixThread::ToErrno(scePthreadSetaffinity(thread, affinity));
}

int APS5_VABI pthread_join_nid_postfix(Pthread thread, void** value) {
    return PosixThread::ToErrno(scePthreadJoin(thread, value));
}

int APS5_VABI pthread_timedjoin_np_nid_postfix(Pthread thread, void** value, const KernelTimespec* abstime) {
    if (abstime == nullptr || abstime->tv_sec < 0 || abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000)
        return PosixThread::GUEST_EINVAL;
    return PosixThread::ToErrno(JoinThread_nid_no_patch(thread, value, abstime));
}

namespace {
struct RegisterPthreadOnLoad {
    RegisterPthreadOnLoad() {
        RegisterPthreadCreate_nid_no_patch(reinterpret_cast<PthreadCreateFn_nid_no_patch>(pthread_create_nid_postfix));
        RegisterPthreadJoin_nid_no_patch(reinterpret_cast<PthreadJoinFn_nid_no_patch>(pthread_join_nid_postfix));
        RegisterPthreadDetach_nid_no_patch(reinterpret_cast<PthreadDetachFn_nid_no_patch>(pthread_detach_nid_postfix));
        static const ThreadForwards_nid_no_patch forwards{
            reinterpret_cast<decltype(forwards.once)>(scePthreadOnce),
            reinterpret_cast<decltype(forwards.keyCreate)>(scePthreadKeyCreate),
            scePthreadKeyDelete,
            scePthreadGetspecific,
            scePthreadSetspecific,
            reinterpret_cast<decltype(forwards.equal)>(scePthreadEqual),
            reinterpret_cast<decltype(forwards.self)>(scePthreadSelf),
            scePthreadExit,
        };
        RegisterThreadForwards_nid_no_patch(&forwards);
        static const PthreadSyncForwards_nid_no_patch sync{
            reinterpret_cast<decltype(sync.mutexattrInit)>(scePthreadMutexattrInit),
            reinterpret_cast<decltype(sync.mutexattrSettype)>(scePthreadMutexattrSettype),
            reinterpret_cast<decltype(sync.mutexattrDestroy)>(scePthreadMutexattrDestroy),
            reinterpret_cast<decltype(sync.mutexInit)>(scePthreadMutexInit),
            reinterpret_cast<decltype(sync.mutexDestroy)>(scePthreadMutexDestroy),
            reinterpret_cast<decltype(sync.mutexLock)>(scePthreadMutexLock),
            reinterpret_cast<decltype(sync.mutexTrylock)>(scePthreadMutexTrylock),
            reinterpret_cast<decltype(sync.mutexUnlock)>(scePthreadMutexUnlock),
            reinterpret_cast<decltype(sync.condInit)>(scePthreadCondInit),
            reinterpret_cast<decltype(sync.condDestroy)>(scePthreadCondDestroy),
            reinterpret_cast<decltype(sync.condWait)>(scePthreadCondWait),
            reinterpret_cast<decltype(sync.condSignal)>(scePthreadCondSignal),
            reinterpret_cast<decltype(sync.condBroadcast)>(scePthreadCondBroadcast),
            reinterpret_cast<decltype(sync.detach)>(scePthreadDetach),
            reinterpret_cast<decltype(sync.join)>(scePthreadJoin),
            scePthreadYield,
        };
        RegisterPthreadSyncForwards_nid_no_patch(&sync);
    }
};
const RegisterPthreadOnLoad gRegisterPthread;
}

int APS5_VABI pthread_rename_np_nid_postfix(Pthread thread, const char* name) {
    if (thread && name && std::strlen(name) >= 32) return PosixThread::ToErrno(SCE_KERNEL_ERROR_ENAMETOOLONG);
    return PosixThread::ToErrno(scePthreadRename(thread, name));
}

int APS5_VABI pthread_getname_np_nid_postfix(Pthread thread, char* name) {
    if (!thread) return PosixThread::GUEST_ESRCH;
    if (!name) return PosixThread::GUEST_EFAULT;
    // A joined thread answers ESRCH on the console (PS5 firmware 13.60); the object is retained
    // after scePthreadJoin, so _joined tells it apart from a live thread.
    if (thread->_joined.load(std::memory_order_acquire)) return PosixThread::GUEST_ESRCH;
    return PosixThread::ToErrno(scePthreadGetname(thread, name));
}

void APS5_VABI pthread_set_name_np_nid_postfix(Pthread thread, const char* name) {
    scePthreadRename(thread, name != nullptr ? name : "");
}

Pthread APS5_VABI pthread_self_nid_postfix(void) {
    return scePthreadSelf();
}

int APS5_VABI scePthreadGetthreadid(void);

int APS5_VABI pthread_getthreadid_np_nid_postfix(void) {
    return scePthreadGetthreadid();
}

int APS5_VABI pthread_equal_nid_postfix(Pthread first, Pthread second) {
    return first == second;
}

int APS5_VABI pthread_getcpuclockid_nid_postfix(Pthread thread, int* clockId) {
    constexpr int guestFault = 14;
    if (!thread) return PosixThread::GUEST_EINVAL;
    if (!clockId) return guestFault;
    *clockId = GuestThreadCpuClockId(thread);
    return 0;
}

int APS5_VABI sched_yield_nid_postfix(void) {
    scePthreadYield();
    return 0;
}

int APS5_VABI pthread_cancel_nid_postfix(Pthread thread) {
    return PosixThread::ToErrno(scePthreadCancel(thread));
}

int APS5_VABI pthread_setcancelstate_nid_postfix(int state, int* old_state) {
    return PosixThread::ToErrno(scePthreadSetcancelstate(state, old_state));
}

int APS5_VABI pthread_setcanceltype_nid_postfix(int type, int* old_type) {
    return PosixThread::ToErrno(scePthreadSetcanceltype(type, old_type));
}

int APS5_VABI pthread_setprio_nid_postfix(Pthread thread, int prio) {
    return PosixThread::ToErrno(scePthreadSetprio(thread, prio));
}

int APS5_VABI pthread_setschedparam_nid_postfix(Pthread thread, int policy, const KernelSchedParam* param) {
    if (!param) return PosixThread::GUEST_EINVAL;
    return PosixThread::ToErrno(scePthreadSetschedparam(thread, policy, param));
}

void APS5_VABI pthread_testcancel_nid_postfix(void) {
    scePthreadTestcancel();
}

void APS5_VABI pthread_yield_nid_postfix(void) {
    scePthreadYield();
}

unsigned int APS5_VABI sleep_nid_postfix(unsigned int seconds) {
    ThreadCancel::Check();
    {
        const auto deadline = TimedWait::DeadlineNanos(static_cast<std::uint64_t>(seconds) * 1000000ULL);
        std::mutex mutex;
        TimedWait::Condition condition;
        std::unique_lock lock(mutex);
        while (!ThreadCancel::Requested()) {
            if (!ThreadCancel::Wait(condition, lock, deadline)) break;
        }
    }
    ThreadCancel::Check();
    return 0;
}

}
