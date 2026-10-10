#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sceKernelCreateSema(KernelSema*, const char*, std::uint32_t, int, int, void*);
int APS5_VABI sceKernelDeleteSema(KernelSema);
int APS5_VABI sceKernelSignalSema(KernelSema, int);
int APS5_VABI sceKernelPollSema(KernelSema, int);
int APS5_VABI sceKernelCreateEqueue(KernelEqueue* eq, const char* name);
int APS5_VABI sceKernelDeleteEqueue(KernelEqueue eq);
int APS5_VABI sceKernelWaitEqueue(KernelEqueue eq, KernelEvent* ev, int num, int* out, const KernelUseconds* timo);
int APS5_VABI sceKernelDeleteUserEvent(KernelEqueue eq, int id);
int APS5_VABI scePthreadMutexattrInit(PthreadMutexattr* attr);
int APS5_VABI pthread_mutexattr_init_nid_postfix(PthreadMutexattr* attr);
int APS5_VABI pthread_mutexattr_settype_nid_postfix(PthreadMutexattr* attr, int type);
int APS5_VABI pthread_mutexattr_destroy_nid_postfix(PthreadMutexattr* attr);
int APS5_VABI pthread_mutex_init_nid_postfix(PthreadMutex* mutex, const PthreadMutexattr* attr);
int APS5_VABI pthread_mutex_lock_nid_postfix(PthreadMutex* mutex);
int APS5_VABI pthread_mutex_unlock_nid_postfix(PthreadMutex* mutex);
int APS5_VABI pthread_mutex_destroy_nid_postfix(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexattrDestroy(PthreadMutexattr* attr);
int APS5_VABI scePthreadMutexattrSettype(PthreadMutexattr* attr, int type);
int APS5_VABI scePthreadMutexattrSetprotocol(PthreadMutexattr* attr, int protocol);
int APS5_VABI scePthreadMutexInit(PthreadMutex* mutex, const PthreadMutexattr* attr, const char* name);
int APS5_VABI scePthreadMutexDestroy(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexLock(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexUnlock(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexTrylock(PthreadMutex* mutex);
}

namespace {

using MutexOperation = int (APS5_VABI *)(PthreadMutex*);
using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;

constexpr int sceOk = 0;
constexpr int sceKernelErrorEnoent = static_cast<int>(0x80020002);
constexpr int sceKernelErrorEbadf = static_cast<int>(0x80020009);
constexpr int sceKernelErrorEdeadlk = static_cast<int>(0x8002000B);
constexpr int sceKernelErrorEfault = static_cast<int>(0x8002000E);
constexpr int sceKernelErrorEinval = static_cast<int>(0x80020016);
constexpr int sceKernelErrorEbusy = static_cast<int>(0x80020010);
constexpr int sceKernelErrorEtimedout = static_cast<int>(0x8002003C);
constexpr int mutexTypeErrorcheck = 1;
constexpr int prioNone = 0;
constexpr int prioInherit = 1;
constexpr int prioProtect = 2;
constexpr int mutexTypeAdaptive = 4;
constexpr int posixEdeadlk = 11;
constexpr int maximum = std::numeric_limits<int>::max();
constexpr KernelUseconds shortTimeout = 1000;

PthreadMutex StaticAdaptiveInitializer() {
    return reinterpret_cast<PthreadMutex>(std::uintptr_t{1});
}

class Semaphore {
public:
    Semaphore(const char* name, int initial, int max) {
        RequireEqual(sceKernelCreateSema(&handle, name, 1, initial, max, nullptr), sceOk, std::string("create semaphore ") + name);
    }

    ~Semaphore() {
        if (!deleted) sceKernelDeleteSema(handle);
    }

    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;

    KernelSema Handle() const noexcept { return handle; }

    int Delete() {
        deleted = true;
        return sceKernelDeleteSema(handle);
    }

private:
    KernelSema handle = nullptr;
    bool deleted = false;
};

class Equeue {
public:
    Equeue() {
        RequireEqual(sceKernelCreateEqueue(&handle, "errors"), sceOk, "create equeue");
    }

    ~Equeue() {
        if (!deleted) sceKernelDeleteEqueue(handle);
    }

    Equeue(const Equeue&) = delete;
    Equeue& operator=(const Equeue&) = delete;

    KernelEqueue Handle() const noexcept { return handle; }

    int Delete() {
        deleted = true;
        return sceKernelDeleteEqueue(handle);
    }

private:
    KernelEqueue handle = 0;
    bool deleted = false;
};

class SceMutexattr {
public:
    SceMutexattr() {
        RequireEqual(scePthreadMutexattrInit(&handle), sceOk, "scePthreadMutexattrInit");
    }

    ~SceMutexattr() {
        if (!destroyed) scePthreadMutexattrDestroy(&handle);
    }

    SceMutexattr(const SceMutexattr&) = delete;
    SceMutexattr& operator=(const SceMutexattr&) = delete;

    PthreadMutexattr* Get() noexcept { return &handle; }

    int Destroy() {
        destroyed = true;
        return scePthreadMutexattrDestroy(&handle);
    }

private:
    PthreadMutexattr handle = nullptr;
    bool destroyed = false;
};

class PosixMutexattr {
public:
    PosixMutexattr() {
        RequireEqual(pthread_mutexattr_init_nid_postfix(&handle), 0, "pthread_mutexattr_init");
    }

    ~PosixMutexattr() {
        if (!destroyed) pthread_mutexattr_destroy_nid_postfix(&handle);
    }

    PosixMutexattr(const PosixMutexattr&) = delete;
    PosixMutexattr& operator=(const PosixMutexattr&) = delete;

    PthreadMutexattr* Get() noexcept { return &handle; }

    int Destroy() {
        destroyed = true;
        return pthread_mutexattr_destroy_nid_postfix(&handle);
    }

private:
    PthreadMutexattr handle = nullptr;
    bool destroyed = false;
};

struct MutexApi {
    MutexOperation lock;
    MutexOperation trylock;
    MutexOperation unlock;
    MutexOperation destroy;
    int ok;
};

const MutexApi sceApi{scePthreadMutexLock, scePthreadMutexTrylock, scePthreadMutexUnlock, scePthreadMutexDestroy, sceOk};
const MutexApi posixApi{pthread_mutex_lock_nid_postfix, nullptr, pthread_mutex_unlock_nid_postfix,
                            pthread_mutex_destroy_nid_postfix, 0};

class Mutex {
public:
    Mutex(const MutexApi& api, PthreadMutex initial) : api(api), handle(initial) {}

    ~Mutex() {
        if (held) api.unlock(&handle);
        if (!destroyed) api.destroy(&handle);
    }

    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;

    PthreadMutex* Get() noexcept { return &handle; }
    PthreadMutex Value() const noexcept { return handle; }

    int Lock() { return Track(api.lock(&handle)); }
    int Trylock() { return Track(api.trylock(&handle)); }

    int Unlock() {
        const int result = api.unlock(&handle);
        if (result == api.ok) held = false;
        return result;
    }

    int Destroy() {
        const int result = api.destroy(&handle);
        if (result == api.ok) destroyed = true;
        return result;
    }

private:
    int Track(int result) {
        if (result == api.ok) held = true;
        return result;
    }

    const MutexApi& api;
    PthreadMutex handle;
    bool held = false;
    bool destroyed = false;
};

void RequireRelockDeadlocks(Mutex& mutex, int ok, int deadlock) {
    RequireEqual(mutex.Lock(), ok, "first lock");
    RequireEqual(mutex.Lock(), deadlock, "relock by owner");
    RequireEqual(mutex.Unlock(), ok, "unlock");
    RequireEqual(mutex.Destroy(), ok, "destroy");
}

const Case semaOverflowRejected{"SignalSema_CountWouldExceedIntMaximum_FailsWithEinval", [] {
    Semaphore semaphore("overflow", maximum - 1, maximum);
    RequireEqual(sceKernelSignalSema(semaphore.Handle(), 2), sceKernelErrorEinval, "signal 2 at maximum - 1");
    RequireEqual(sceKernelSignalSema(semaphore.Handle(), maximum), sceKernelErrorEinval, "signal maximum at maximum - 1");
}};

const Case semaReachesMaximum{"SignalSema_ToIntMaximumFromZero_SucceedsAndFurtherSignalFailsWithEinval", [] {
    Semaphore semaphore("overflow", maximum - 1, maximum);
    RequireEqual(sceKernelPollSema(semaphore.Handle(), maximum - 1), sceOk, "poll maximum - 1");
    RequireEqual(sceKernelSignalSema(semaphore.Handle(), maximum), sceOk, "signal maximum at zero");
    RequireEqual(sceKernelSignalSema(semaphore.Handle(), 1), sceKernelErrorEinval, "signal 1 at maximum");
    RequireEqual(sceKernelPollSema(semaphore.Handle(), maximum), sceOk, "poll maximum");
    RequireEqual(semaphore.Delete(), sceOk, "delete");
}};

const Case semaLimit{"SignalSema_SmallMaxCount_RejectsOverflowAndAcceptsUpToMax", [] {
    Semaphore semaphore("limit", 1, 3);
    RequireEqual(sceKernelSignalSema(semaphore.Handle(), 3), sceKernelErrorEinval, "signal 3 at 1 of 3");
    RequireEqual(sceKernelSignalSema(semaphore.Handle(), 2), sceOk, "signal 2 at 1 of 3");
    RequireEqual(sceKernelPollSema(semaphore.Handle(), 3), sceOk, "poll 3");
    RequireEqual(semaphore.Delete(), sceOk, "delete");
}};

const Case waitEmpty{"WaitEqueue_EmptyQueue_TimesOutWithZeroCount", [] {
    Equeue queue;
    KernelEvent event{};
    int count = -1;
    RequireEqual(sceKernelWaitEqueue(queue.Handle(), &event, 1, &count, &shortTimeout), sceKernelErrorEtimedout, "wait");
    RequireEqual(count, 0, "event count");
}};

const Case waitNullEvents{"WaitEqueue_NullEvents_FailsWithEfault", [] {
    Equeue queue;
    int count = -1;
    RequireEqual(sceKernelWaitEqueue(queue.Handle(), nullptr, 1, &count, &shortTimeout), sceKernelErrorEfault, "wait");
}};

const Case waitZeroEvents{"WaitEqueue_ZeroEventCapacity_FailsWithEinval", [] {
    Equeue queue;
    KernelEvent event{};
    int count = -1;
    RequireEqual(sceKernelWaitEqueue(queue.Handle(), &event, 0, &count, &shortTimeout), sceKernelErrorEinval, "wait");
}};

const Case deleteMissingUserEvent{"DeleteUserEvent_NotAdded_FailsWithEnoent", [] {
    Equeue queue;
    RequireEqual(sceKernelDeleteUserEvent(queue.Handle(), 7), sceKernelErrorEnoent, "delete user event 7");
}};

const Case deleteEqueueTwice{"DeleteEqueue_AlreadyDeleted_FailsWithEbadf", [] {
    Equeue queue;
    RequireEqual(queue.Delete(), sceOk, "first delete");
    RequireEqual(sceKernelDeleteEqueue(queue.Handle()), sceKernelErrorEbadf, "second delete");
}};

const Case waitDeleted{"WaitEqueue_DeletedQueue_FailsWithEbadf", [] {
    Equeue queue;
    RequireEqual(queue.Delete(), sceOk, "delete");
    KernelEvent event{};
    int count = -1;
    RequireEqual(sceKernelWaitEqueue(queue.Handle(), &event, 1, &count, &shortTimeout), sceKernelErrorEbadf, "wait");
}};

const Case createNullEqueue{"CreateEqueue_NullOutput_FailsWithEinval", [] {
    RequireEqual(sceKernelCreateEqueue(nullptr, "errors"), sceKernelErrorEinval, "create");
}};

const Case settypeErrorcheck{"MutexattrSettype_Errorcheck_Succeeds", [] {
    SceMutexattr attr;
    RequireEqual(scePthreadMutexattrSettype(attr.Get(), mutexTypeErrorcheck), sceOk, "settype errorcheck");
}};

const Case settypeInvalid{"MutexattrSettype_OutOfRangeType_FailsWithEinval", [] {
    SceMutexattr attr;
    for (const int type : {0, 5}) {
        RequireEqual(scePthreadMutexattrSettype(attr.Get(), type), sceKernelErrorEinval, "type " + std::to_string(type));
    }
}};

const Case setprotocolSupported{"MutexattrSetprotocol_NoneAndInherit_Succeed", [] {
    SceMutexattr attr;
    RequireEqual(scePthreadMutexattrSetprotocol(attr.Get(), prioNone), sceOk, "protocol none");
    RequireEqual(scePthreadMutexattrSetprotocol(attr.Get(), prioInherit), sceOk, "protocol inherit");
}};

const Case setprotocolProtect{"MutexattrSetprotocol_Protect_ThrowsInvalidArgument", [] {
    SceMutexattr attr;
    RequireThrows<std::invalid_argument>([&attr] { scePthreadMutexattrSetprotocol(attr.Get(), prioProtect); },
                                         "protocol protect");
}};

void InitErrorcheckInheritMutex(Mutex& mutex) {
    SceMutexattr attr;
    RequireEqual(scePthreadMutexattrSettype(attr.Get(), mutexTypeErrorcheck), sceOk, "settype errorcheck");
    RequireEqual(scePthreadMutexattrSetprotocol(attr.Get(), prioInherit), sceOk, "protocol inherit");
    RequireEqual(scePthreadMutexInit(mutex.Get(), attr.Get(), nullptr), sceOk, "init");
    RequireEqual(attr.Destroy(), sceOk, "attr destroy");
}

const Case errorcheckRelock{"SceMutexLock_ErrorcheckRelockByOwner_FailsWithEdeadlk", [] {
    Mutex mutex(sceApi, nullptr);
    InitErrorcheckInheritMutex(mutex);
    RequireRelockDeadlocks(mutex, sceOk, sceKernelErrorEdeadlk);
}};

const Case destroyLocked{"SceMutexDestroy_LockedErrorcheckMutex_FailsWithEbusy", [] {
    Mutex mutex(sceApi, nullptr);
    InitErrorcheckInheritMutex(mutex);
    RequireEqual(mutex.Lock(), sceOk, "lock");
    RequireEqual(mutex.Destroy(), sceKernelErrorEbusy, "destroy while locked");
    RequireEqual(mutex.Unlock(), sceOk, "unlock");
    RequireEqual(mutex.Destroy(), sceOk, "destroy after unlock");
}};

const Case adaptiveRelock{"SceMutexLock_AdaptiveRelockByOwner_FailsWithEdeadlk", [] {
    Mutex mutex(sceApi, nullptr);
    SceMutexattr attr;
    RequireEqual(scePthreadMutexattrSettype(attr.Get(), mutexTypeAdaptive), sceOk, "settype adaptive");
    RequireEqual(scePthreadMutexInit(mutex.Get(), attr.Get(), nullptr), sceOk, "init");
    RequireEqual(attr.Destroy(), sceOk, "attr destroy");
    RequireRelockDeadlocks(mutex, sceOk, sceKernelErrorEdeadlk);
}};

const Case staticAdaptiveLock{"SceMutexLock_StaticAdaptiveInitializer_InitializesAndRejectsRelock", [] {
    Mutex mutex(sceApi, StaticAdaptiveInitializer());
    RequireEqual(mutex.Lock(), sceOk, "lock");
    Require(mutex.Value() != StaticAdaptiveInitializer(), "lock replaces the static initializer");
    RequireEqual(mutex.Lock(), sceKernelErrorEdeadlk, "relock");
    RequireEqual(mutex.Trylock(), sceKernelErrorEbusy, "trylock while locked");
    RequireEqual(mutex.Unlock(), sceOk, "unlock");
    RequireEqual(mutex.Destroy(), sceOk, "destroy");
}};

const Case staticAdaptiveTrylock{"SceMutexTrylock_StaticAdaptiveInitializer_InitializesAndRejectsSecondTrylock", [] {
    Mutex mutex(sceApi, StaticAdaptiveInitializer());
    RequireEqual(mutex.Trylock(), sceOk, "trylock");
    RequireEqual(mutex.Trylock(), sceKernelErrorEbusy, "second trylock");
    RequireEqual(mutex.Unlock(), sceOk, "unlock");
    RequireEqual(mutex.Destroy(), sceOk, "destroy");
}};

const Case staticAdaptiveDestroy{"SceMutexDestroy_UnusedStaticAdaptiveInitializer_Succeeds", [] {
    Mutex mutex(sceApi, StaticAdaptiveInitializer());
    RequireEqual(mutex.Destroy(), sceOk, "destroy");
}};

const Case defaultAttrRelock{"SceMutexLock_DefaultAttributeRelockByOwner_FailsWithEdeadlk", [] {
    Mutex mutex(sceApi, nullptr);
    SceMutexattr attr;
    RequireEqual(scePthreadMutexInit(mutex.Get(), attr.Get(), nullptr), sceOk, "init");
    RequireEqual(attr.Destroy(), sceOk, "attr destroy");
    RequireRelockDeadlocks(mutex, sceOk, sceKernelErrorEdeadlk);
}};

const Case nullAttrRelock{"SceMutexLock_NullAttributeRelockByOwner_FailsWithEdeadlk", [] {
    Mutex mutex(sceApi, nullptr);
    RequireEqual(scePthreadMutexInit(mutex.Get(), nullptr, nullptr), sceOk, "init");
    RequireRelockDeadlocks(mutex, sceOk, sceKernelErrorEdeadlk);
}};

const Case posixAdaptiveRelock{"PthreadMutexLock_AdaptiveRelockByOwner_FailsWithEdeadlk", [] {
    Mutex mutex(posixApi, nullptr);
    PosixMutexattr attr;
    RequireEqual(pthread_mutexattr_settype_nid_postfix(attr.Get(), mutexTypeAdaptive), 0, "settype adaptive");
    RequireEqual(pthread_mutex_init_nid_postfix(mutex.Get(), attr.Get()), 0, "init");
    RequireEqual(attr.Destroy(), 0, "attr destroy");
    RequireRelockDeadlocks(mutex, 0, posixEdeadlk);
}};

const Case posixStaticAdaptiveRelock{"PthreadMutexLock_StaticAdaptiveInitializerRelock_FailsWithEdeadlk", [] {
    Mutex mutex(posixApi, StaticAdaptiveInitializer());
    RequireRelockDeadlocks(mutex, 0, posixEdeadlk);
}};

} // namespace
