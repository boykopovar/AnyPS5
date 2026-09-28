#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI scePthreadDetach(Pthread thread);
void APS5_VABI scePthreadExit(void* retval);
Pthread APS5_VABI scePthreadSelf();
int APS5_VABI scePthreadMutexattrInit(PthreadMutexattr* attr);
int APS5_VABI scePthreadMutexattrDestroy(PthreadMutexattr* attr);
int APS5_VABI scePthreadMutexattrSettype(PthreadMutexattr* attr, int type);
int APS5_VABI scePthreadMutexInit(PthreadMutex* mutex, const PthreadMutexattr* attr, const char* name);
int APS5_VABI scePthreadMutexDestroy(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexLock(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexUnlock(PthreadMutex* mutex);
}

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EINVAL = 0x80020016;
static constexpr int SCE_KERNEL_ERROR_EPERM = 0x80020001;
static constexpr int MUTEX_TYPE_RECURSIVE = 2;
static constexpr std::intptr_t WorkerRetval = 0x1234;

static void Require(bool value) { if (!value) std::abort(); }

struct WorkerContext {
    Pthread thread = nullptr;
    Pthread selfFromWorker = nullptr;
    PthreadMutex* mutex = nullptr;
    int unlockResult = 0;
};

static void* APS5_VABI Worker(void* arg) {
    auto& context = *static_cast<WorkerContext*>(arg);
    context.selfFromWorker = scePthreadSelf();
    context.unlockResult = scePthreadMutexUnlock(context.mutex);
    scePthreadExit(reinterpret_cast<void*>(WorkerRetval));
    return nullptr;
}

int main() {
    const Pthread mainSelf = scePthreadSelf();
    Require(mainSelf != nullptr);
    Require(scePthreadSelf() == mainSelf);
    Require(scePthreadJoin(mainSelf, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadDetach(mainSelf) == SCE_KERNEL_ERROR_EINVAL);

    PthreadMutexattr attr = nullptr;
    Require(scePthreadMutexattrInit(&attr) == SCE_OK);
    Require(scePthreadMutexattrSettype(&attr, MUTEX_TYPE_RECURSIVE) == SCE_OK);
    PthreadMutex mutex = nullptr;
    Require(scePthreadMutexInit(&mutex, &attr, nullptr) == SCE_OK);
    Require(scePthreadMutexattrDestroy(&attr) == SCE_OK);

    Require(scePthreadMutexLock(&mutex) == SCE_OK);

    WorkerContext context;
    context.mutex = &mutex;
    Require(scePthreadCreate(&context.thread, nullptr, Worker, &context, nullptr) == SCE_OK);
    Require(context.thread != nullptr);
    Require(context.thread != mainSelf);

    void* result = nullptr;
    Require(scePthreadJoin(context.thread, &result) == SCE_OK);
    Require(reinterpret_cast<std::intptr_t>(result) == WorkerRetval);
    Require(context.selfFromWorker != nullptr);
    Require(context.selfFromWorker == context.thread);
    Require(context.selfFromWorker != mainSelf);
    Require(context.unlockResult == SCE_KERNEL_ERROR_EPERM);

    Require(scePthreadMutexUnlock(&mutex) == SCE_OK);
    Require(scePthreadMutexDestroy(&mutex) == SCE_OK);
}
