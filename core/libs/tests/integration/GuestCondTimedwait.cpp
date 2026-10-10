#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <atomic>
#include <cstdint>
#include <string>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI scePthreadMutexInit(PthreadMutex* mutex, const PthreadMutexattr* attr, const char* name);
int APS5_VABI scePthreadMutexDestroy(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexLock(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexUnlock(PthreadMutex* mutex);
int APS5_VABI scePthreadCondInit(PthreadCond* cond, const PthreadCondattr* attr, const char* name);
int APS5_VABI scePthreadCondDestroy(PthreadCond* cond);
int APS5_VABI scePthreadCondTimedwait(PthreadCond* cond, PthreadMutex* mutex, unsigned int usec);
int APS5_VABI __cxa_atexit_nid_postfix(void (APS5_VABI *)(void*), void*, void*);
void APS5_VABI __pthread_cxa_finalize_nid_postfix(void*);
unsigned int APS5_VABI sceKernelSleep(unsigned int seconds);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int SCE_OK = 0;
constexpr int SCE_KERNEL_ERROR_ETIMEDOUT = static_cast<int>(0x8002003C);

struct Context {
    PthreadMutex mutex = nullptr;
    std::atomic<bool> acquired{false};
    std::atomic<int> lockResult{-1};
    std::atomic<int> unlockResult{-1};
};

void* APS5_VABI Contender(void* arg) {
    auto& context = *static_cast<Context*>(arg);
    context.lockResult.store(scePthreadMutexLock(&context.mutex));
    context.acquired.store(true);
    context.unlockResult.store(scePthreadMutexUnlock(&context.mutex));
    return nullptr;
}

class CondFixture {
public:
    CondFixture() {
        RequireEqual(scePthreadMutexInit(&context.mutex, nullptr, nullptr), SCE_OK, "mutex init");
        mutexReady = true;
        RequireEqual(scePthreadCondInit(&cond, nullptr, nullptr), SCE_OK, "cond init");
        condReady = true;
    }
    ~CondFixture() {
        if (locked) scePthreadMutexUnlock(&context.mutex);
        if (thread != nullptr) scePthreadJoin(thread, nullptr);
        if (condReady) scePthreadCondDestroy(&cond);
        if (mutexReady) scePthreadMutexDestroy(&context.mutex);
    }
    CondFixture(const CondFixture&) = delete;
    CondFixture& operator=(const CondFixture&) = delete;

    void LockAndStartContender() {
        RequireEqual(scePthreadMutexLock(&context.mutex), SCE_OK, "main lock");
        locked = true;
        RequireEqual(scePthreadCreate(&thread, nullptr, Contender, &context, nullptr), SCE_OK, "create contender");
    }

    int Unlock() {
        locked = false;
        return scePthreadMutexUnlock(&context.mutex);
    }

    int Join() {
        const int result = scePthreadJoin(thread, nullptr);
        thread = nullptr;
        return result;
    }

    int DestroyCond() {
        condReady = false;
        return scePthreadCondDestroy(&cond);
    }

    int DestroyMutex() {
        mutexReady = false;
        return scePthreadMutexDestroy(&context.mutex);
    }

    Context context;
    PthreadCond cond = nullptr;

private:
    Pthread thread = nullptr;
    bool locked = false;
    bool mutexReady = false;
    bool condReady = false;
};

int cleanupCalls = 0;

void APS5_VABI Cleanup(void*) {
    ++cleanupCalls;
}

const Case releasesMutex{"CondTimedwait_MutexContended_TimesOutAndLetsContenderAcquire", [] {
    CondFixture fixture;
    fixture.LockAndStartContender();
    int waits = 0;
    while (!fixture.context.acquired.load()) {
        RequireEqual(scePthreadCondTimedwait(&fixture.cond, &fixture.context.mutex, 1000), SCE_KERNEL_ERROR_ETIMEDOUT,
                     "timed wait " + std::to_string(waits++));
    }
    RequireEqual(fixture.Unlock(), SCE_OK, "main unlock");
    RequireEqual(fixture.Join(), SCE_OK, "join contender");
    RequireEqual(fixture.context.lockResult.load(), SCE_OK, "contender lock");
    RequireEqual(fixture.context.unlockResult.load(), SCE_OK, "contender unlock");
    RequireEqual(fixture.DestroyCond(), SCE_OK, "cond destroy");
    RequireEqual(fixture.DestroyMutex(), SCE_OK, "mutex destroy");
}};

const Case zeroSleep{"KernelSleep_ZeroSeconds_ReturnsOk", [] {
    RequireEqual(sceKernelSleep(0), static_cast<unsigned int>(SCE_OK), "sleep(0)");
}};

const Case finalize{"PthreadCxaFinalize_RegisteredHandle_RunsAtexitHandler", [] {
    cleanupCalls = 0;
    void* finalizeHandle = reinterpret_cast<void*>(7);
    RequireEqual(__cxa_atexit_nid_postfix(Cleanup, nullptr, finalizeHandle), SCE_OK, "register handler");
    __pthread_cxa_finalize_nid_postfix(finalizeHandle);
    RequireEqual(cleanupCalls, 1, "handler calls");
}};

} // namespace
