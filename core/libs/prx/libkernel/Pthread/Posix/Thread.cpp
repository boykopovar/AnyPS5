#include "prx/libkernel/Pthread/include/ThreadLifecycle.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
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
#include <stdexcept>
#include <string>
#include <thread>

extern "C" {

int APS5_VABI JoinThread_nid_no_patch(Pthread thread, void** retval, const KernelTimespec* abstime);
bool IsInitialPthread_nid_no_patch();

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

int APS5_VABI cpuset_getaffinity_nid_postfix(int level, int which, std::int64_t id, std::size_t size, void* mask) {
    constexpr int LevelWhich = 3, WhichThread = 1, WhichProcess = 2;
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
    if (which == WhichThread) {
        const auto tid = id == -1 ? self->tid : id;
        if (!GetLiveGuestThreadAffinity_nid_no_patch(tid, affinity)) return fail(GuestEsrch);
    } else if (which == WhichProcess) {
        if (id != -1 && id != getpid_nid_postfix()) return fail(GuestEsrch);
    } else if (which >= 3 && which <= 6) {
        NotImplemented_nid_no_patch("cpuset_getaffinity other than the calling thread or process");
    } else {
        return fail(GuestEinval);
    }
    if (level < 1 || level > 3) return fail(GuestEinval);
    if (level != LevelWhich) NotImplemented_nid_no_patch("cpuset_getaffinity other than the calling thread or process");
    if (which == WhichProcess) affinity = LiveGuestThreadsAffinity();
    std::array<std::uint8_t, MaximumSize> buffer{};
    std::memcpy(buffer.data(), &affinity, sizeof(affinity));
    if (!GuestCopyOut_nid_no_patch(mask, buffer.data(), size)) return fail(GuestEfault);
    return 0;
}

int APS5_VABI cpuset_setaffinity_nid_postfix(int level, int which, std::int64_t id, std::size_t size, const void* mask) {
    constexpr int LevelWhich = 3, WhichThread = 1, WhichProcess = 2;
    constexpr int GuestEsrch = 3, GuestEdeadlk = 11, GuestEfault = 14, GuestEinval = 22, GuestErange = 34;
    constexpr std::size_t MaximumSize = 256 / 8;
    const auto fail = [](int error) {
        *__error_nid_postfix() = error;
        return -1;
    };
    if (size < sizeof(KernelCpumask) || size > MaximumSize) return fail(GuestErange);
    if (!mask) return fail(GuestEfault);
    std::array<std::uint8_t, MaximumSize> buffer{};
    if (!GuestCopyIn_nid_no_patch(buffer.data(), mask, size)) return fail(GuestEfault);
    for (std::size_t index = sizeof(KernelCpumask); index < size; ++index) {
        if (buffer[index] != 0) return fail(GuestEinval);
    }
    auto* self = scePthreadSelf();
    if (which == WhichThread) {
        if (id != -1 && id != self->tid && !LiveGuestThreadExists(id)) return fail(GuestEsrch);
    } else if (which == WhichProcess) {
        if (id != -1 && id != getpid_nid_postfix()) return fail(GuestEsrch);
    } else if (which >= 3 && which <= 6) {
        NotImplemented_nid_no_patch("cpuset_setaffinity other than the calling thread or process");
    } else {
        return fail(GuestEinval);
    }
    if (level < 1 || level > 3) return fail(GuestEinval);
    if (level != LevelWhich) NotImplemented_nid_no_patch("cpuset_setaffinity other than the calling thread or process");
    KernelCpumask requested = 0;
    std::memcpy(&requested, buffer.data(), sizeof(requested));
    const KernelCpumask affinity = requested & sceKernelGetAvailableCpumask();
    if (affinity == 0) return fail(GuestEdeadlk);
    if (which == WhichThread) {
        const auto tid = id == -1 ? self->tid : id;
        if (!SetLiveGuestThreadAffinity_nid_no_patch(tid, affinity)) return fail(GuestEsrch);
    } else SetLiveGuestThreadsAffinity(affinity);
    return 0;
}

int APS5_VABI rtprio_thread_nid_postfix(int function, int lwpid, GuestRtPrio* priority) {
    constexpr int Lookup = 0, Set = 1;
    constexpr int GuestEperm = 1, GuestEsrch = 3, GuestEfault = 14;
    constexpr int GuestSchedFifo = 1, GuestSchedRr = 3;
    constexpr std::uint16_t Realtime = 2, Normal = 3, Idle = 4, Fifo = 10, FifoBit = 8;
    const auto fail = [](int error) {
        *__error_nid_postfix() = error;
        return -1;
    };
    if (function != Lookup && function != Set) return fail(GuestEsrch);
    if (priority == nullptr) return fail(GuestEfault);
    auto* self = scePthreadSelf();
    int policy = self->schedPolicy.load(std::memory_order_relaxed);
    int schedPriority = self->priority.load(std::memory_order_relaxed);
    const bool other = lwpid != 0 && static_cast<std::int64_t>(lwpid) != static_cast<std::int64_t>(self->tid);
    if (other && !GetLiveGuestThreadSched_nid_no_patch(lwpid, policy, schedPriority)) return fail(GuestEsrch);
    GuestRtPrio requested{};
    if (function == Set && !GuestCopyIn_nid_no_patch(&requested, priority, sizeof(requested))) return fail(GuestEfault);
    const bool realTime = policy == GuestSchedFifo || policy == GuestSchedRr;
    if (function == Lookup) {
        GuestRtPrio current{};
        current.type = realTime ? (policy == GuestSchedFifo ? Fifo : Realtime) : Normal;
        current.prio = realTime ? static_cast<std::uint16_t>(schedPriority) : 0;
        if (!GuestCopyOut_nid_no_patch(priority, &current, sizeof(current))) return fail(GuestEfault);
        return 0;
    }
    const auto requestedClass = static_cast<std::uint16_t>(requested.type & ~FifoBit);
    if (requestedClass == Realtime || requestedClass == Idle) return fail(GuestEperm);
    if (other) NotImplemented_nid_no_patch("rtprio_thread: a thread other than the calling one");
    if (realTime) NotImplemented_nid_no_patch("rtprio_thread: a guest thread's real-time class");
    if (requested.type == Normal && requested.prio == 0) return 0;
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

static std::unique_lock<std::mutex> SchedTarget(int pid, Pthread& thread, const char* function) {
    if (pid == 0) {
        thread = scePthreadSelf();
        return LockLiveGuestThread(thread);
    }
    if (pid != getpid_nid_postfix()) return {};
    if (IsInitialPthread_nid_no_patch()) {
        thread = scePthreadSelf();
        return LockLiveGuestThread(thread);
    }
#ifndef __APPLE__
    auto lock = LockMainGuestThread_nid_no_patch(thread);
    if (lock.owns_lock()) return lock;
    throw std::runtime_error(std::string(function) + ": the own pid selects the main thread, which has not called scePthreadSelf or has exited");
#else
    throw std::runtime_error(std::string(function) + ": the own pid selects the guest main thread, which is not identified on macOS");
#endif
}

static int SchedFailure(int error) {
    *__error_nid_postfix() = error;
    return -1;
}

static bool InSchedRange(int policy, int priority) {
    return priority >= sched_get_priority_min_nid_postfix(policy) && priority <= sched_get_priority_max_nid_postfix(policy);
}

int APS5_VABI sched_getparam_nid_postfix(int pid, KernelSchedParam* param) {
    KernelSchedParam current{};
    {
        Pthread thread = nullptr;
        const auto live = SchedTarget(pid, thread, "sched_getparam");
        if (!live.owns_lock()) return SchedFailure(PosixThread::GUEST_ESRCH);
        current.sched_priority = thread->priority.load(std::memory_order_relaxed);
    }
    if (!param || !GuestCopyOut_nid_no_patch(param, &current, sizeof(current))) return SchedFailure(PosixThread::GUEST_EFAULT);
    return 0;
}

int APS5_VABI sched_getscheduler_nid_postfix(int pid) {
    Pthread thread = nullptr;
    const auto live = SchedTarget(pid, thread, "sched_getscheduler");
    if (!live.owns_lock()) return SchedFailure(PosixThread::GUEST_ESRCH);
    return thread->schedPolicy.load(std::memory_order_relaxed);
}

int APS5_VABI sched_setparam_nid_postfix(int pid, const KernelSchedParam* param) {
    if (!param) return SchedFailure(PosixThread::GUEST_EFAULT);
    KernelSchedParam requested{};
    if (!GuestCopyIn_nid_no_patch(&requested, param, sizeof(requested))) return SchedFailure(PosixThread::GUEST_EFAULT);
    Pthread thread = nullptr;
    const auto live = SchedTarget(pid, thread, "sched_setparam");
    if (!live.owns_lock()) return SchedFailure(PosixThread::GUEST_ESRCH);
    const int policy = thread->schedPolicy.load(std::memory_order_relaxed);
    if (policy != GUEST_SCHED_FIFO && policy != GUEST_SCHED_RR)
        throw std::runtime_error("sched_setparam: the thread's policy " + std::to_string(policy) + " is neither SCHED_FIFO nor SCHED_RR, for which the answer of a PS5 is unmeasured");
    if (!InSchedRange(policy, requested.sched_priority)) return SchedFailure(PosixThread::GUEST_EPERM);
    thread->priority.store(requested.sched_priority, std::memory_order_relaxed);
    return 0;
}

int APS5_VABI pthread_getprio_nid_postfix(Pthread thread) {
    if (!thread) {
        *__error_nid_postfix() = PosixThread::GUEST_EINVAL;
        return -1;
    }
    const auto lock = LockLiveGuestThread(thread);
    if (!lock.owns_lock()) {
        *__error_nid_postfix() = PosixThread::GUEST_ESRCH;
        return -1;
    }
    return thread->priority.load(std::memory_order_relaxed);
}

int APS5_VABI pthread_getaffinity_np_nid_postfix(Pthread thread, std::size_t cpusetSize, void* cpuset) {
    if (!thread) return PosixThread::GUEST_EINVAL;
    if (cpusetSize >= GUEST_CPUSET_BYTES) return GUEST_ERANGE;
    if (cpusetSize != 8 && cpusetSize != 16) NotImplemented_nid_no_patch("pthread_getaffinity_np with a set other than 8, 16 or 32 bytes and more");
    if (!cpuset) return PosixThread::GUEST_EFAULT;
    KernelCpumask mask = 0;
    const int result = scePthreadGetaffinity(thread, &mask);
    if (result != 0) return PosixThread::ToErrno(result);
    std::array<std::uint8_t, GUEST_CPUSET_BYTES> buffer{};
    std::memcpy(buffer.data(), &mask, sizeof(mask));
    if (!GuestCopyOut_nid_no_patch(cpuset, buffer.data(), cpusetSize)) return PosixThread::GUEST_EFAULT;
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
    if (!GuestCopyIn_nid_no_patch(&requested, cpuset, sizeof(requested))) return PosixThread::GUEST_EFAULT;
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
        RegisterProcessorCount_nid_no_patch(static_cast<unsigned>(std::popcount(DEFAULT_THREAD_AFFINITY)));
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
