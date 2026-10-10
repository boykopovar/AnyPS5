#include "SceTypes.hpp"
#include <atomic>
#include <cstdint>
#include <cstdlib>

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

static int cleanupAt = -1;
static void APS5_VABI Cleanup(void*) { cleanupAt = 0; }

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_ETIMEDOUT = static_cast<int>(0x8002003C);

static void Require(bool value) { if (!value) std::abort(); }

struct Context {
    PthreadMutex mutex = nullptr;
    std::atomic<bool> acquired{false};
};

static void* APS5_VABI Contender(void* arg) {
    auto& context = *static_cast<Context*>(arg);
    Require(scePthreadMutexLock(&context.mutex) == SCE_OK);
    context.acquired.store(true);
    Require(scePthreadMutexUnlock(&context.mutex) == SCE_OK);
    return nullptr;
}

int main() {
    Context context;
    PthreadCond cond = nullptr;
    Require(scePthreadMutexInit(&context.mutex, nullptr, nullptr) == SCE_OK);
    Require(scePthreadCondInit(&cond, nullptr, nullptr) == SCE_OK);

    Require(scePthreadMutexLock(&context.mutex) == SCE_OK);
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Contender, &context, nullptr) == SCE_OK);
    while (!context.acquired.load())
        Require(scePthreadCondTimedwait(&cond, &context.mutex, 1000) == SCE_KERNEL_ERROR_ETIMEDOUT);
    Require(scePthreadMutexUnlock(&context.mutex) == SCE_OK);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);

    Require(scePthreadCondDestroy(&cond) == SCE_OK);
    Require(scePthreadMutexDestroy(&context.mutex) == SCE_OK);

    Require(sceKernelSleep(0) == SCE_OK);
    void* finalizeHandle = reinterpret_cast<void*>(7);
    Require(__cxa_atexit_nid_postfix(Cleanup, nullptr, finalizeHandle) == SCE_OK);
    __pthread_cxa_finalize_nid_postfix(finalizeHandle);
    Require(cleanupAt == 0);
}
