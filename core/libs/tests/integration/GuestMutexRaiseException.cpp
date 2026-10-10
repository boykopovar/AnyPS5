#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <atomic>
#include <chrono>
#include <thread>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread*, const PthreadAttr*, PthreadEntry, void*, const char*);
int APS5_VABI scePthreadJoin(Pthread, void**);
int APS5_VABI scePthreadMutexInit(PthreadMutex*, const PthreadMutexattr*, const char*);
int APS5_VABI scePthreadMutexDestroy(PthreadMutex*);
int APS5_VABI scePthreadMutexLock(PthreadMutex*);
int APS5_VABI scePthreadMutexUnlock(PthreadMutex*);
int APS5_VABI sceKernelInstallExceptionHandler(int, void*);
int APS5_VABI sceKernelRemoveExceptionHandler(int);
int APS5_VABI sceKernelRaiseException(Pthread, int);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int sigUsr1 = 30;

std::atomic<int> deliveries{0};

void APS5_VABI Handler(int, void*) {
    deliveries.fetch_add(1);
}

class InstalledHandler {
public:
    InstalledHandler() {
        deliveries.store(0);
        RequireEqual(sceKernelInstallExceptionHandler(sigUsr1, reinterpret_cast<void*>(&Handler)), 0, "install handler");
    }
    ~InstalledHandler() { sceKernelRemoveExceptionHandler(sigUsr1); }
    InstalledHandler(const InstalledHandler&) = delete;
    InstalledHandler& operator=(const InstalledHandler&) = delete;
};

struct Context {
    PthreadMutex mutex = nullptr;
    std::atomic<bool> started{false};
    std::atomic<bool> acquired{false};
    std::atomic<int> lockResult{-1};
    std::atomic<int> unlockResult{-1};
};

void* APS5_VABI Worker(void* opaque) {
    auto& context = *static_cast<Context*>(opaque);
    context.started.store(true);
    context.lockResult.store(scePthreadMutexLock(&context.mutex));
    context.acquired.store(true);
    context.unlockResult.store(scePthreadMutexUnlock(&context.mutex));
    return nullptr;
}

class LockedMutex {
public:
    LockedMutex() {
        RequireEqual(scePthreadMutexInit(&context.mutex, nullptr, nullptr), 0, "mutex init");
        RequireEqual(scePthreadMutexLock(&context.mutex), 0, "main thread lock");
        locked = true;
    }
    ~LockedMutex() {
        if (locked) scePthreadMutexUnlock(&context.mutex);
        if (worker != nullptr) scePthreadJoin(worker, nullptr);
        if (!destroyed) scePthreadMutexDestroy(&context.mutex);
    }
    LockedMutex(const LockedMutex&) = delete;
    LockedMutex& operator=(const LockedMutex&) = delete;

    void StartWaiter() {
        RequireEqual(scePthreadCreate(&worker, nullptr, Worker, &context, "mutex wait"), 0, "create waiter");
        while (!context.started.load()) std::this_thread::yield();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    int Unlock() {
        locked = false;
        return scePthreadMutexUnlock(&context.mutex);
    }

    int Join() {
        const int result = scePthreadJoin(worker, nullptr);
        worker = nullptr;
        return result;
    }

    int Destroy() {
        destroyed = true;
        return scePthreadMutexDestroy(&context.mutex);
    }

    Pthread worker = nullptr;
    Context context;

private:
    bool locked = false;
    bool destroyed = false;
};

const Case raiseDuringMutexWait{"RaiseException_ThreadBlockedOnMutex_DeliversOnceAndThreadAcquiresAfterUnlock", [] {
    const InstalledHandler handler;
    LockedMutex mutex;
    mutex.StartWaiter();
    RequireEqual(sceKernelRaiseException(mutex.worker, sigUsr1), 0, "raise exception");
    for (int i = 0; i < 5000 && deliveries.load() == 0; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    RequireEqual(deliveries.load(), 1, "handler deliveries");
    RequireEqual(mutex.Unlock(), 0, "main thread unlock");
    RequireEqual(mutex.Join(), 0, "join waiter");
    RequireEqual(mutex.context.lockResult.load(), 0, "waiter lock result");
    RequireEqual(mutex.context.unlockResult.load(), 0, "waiter unlock result");
    Require(mutex.context.acquired.load(), "waiter acquired the mutex");
    RequireEqual(mutex.Destroy(), 0, "mutex destroy");
    RequireEqual(sceKernelRemoveExceptionHandler(sigUsr1), 0, "remove handler");
}};

} // namespace
