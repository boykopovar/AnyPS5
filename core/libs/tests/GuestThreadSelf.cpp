#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI scePthreadDetach(Pthread thread);
void APS5_VABI scePthreadExit(void* retval);
Pthread APS5_VABI scePthreadSelf();
void APS5_VABI scePthreadTestcancel();
void APS5_VABI pthread_testcancel_nid_postfix(void);
int APS5_VABI scePthreadSetcancelstate(int state, int* old_state);
int APS5_VABI scePthreadSetcanceltype(int type, int* old_type);
int APS5_VABI scePthreadMutexattrInit(PthreadMutexattr* attr);
int APS5_VABI scePthreadMutexattrDestroy(PthreadMutexattr* attr);
int APS5_VABI scePthreadMutexattrSettype(PthreadMutexattr* attr, int type);
int APS5_VABI scePthreadMutexInit(PthreadMutex* mutex, const PthreadMutexattr* attr, const char* name);
int APS5_VABI scePthreadMutexDestroy(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexLock(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexUnlock(PthreadMutex* mutex);
int APS5_VABI scePthreadAttrInit(PthreadAttr* attr);
int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr);
int APS5_VABI scePthreadAttrGet(Pthread thread, PthreadAttr* attr);
int APS5_VABI scePthreadAttrGetstackaddr(const PthreadAttr* attr, void** stack_addr);
int APS5_VABI scePthreadAttrGetstacksize(const PthreadAttr* attr, size_t* stack_size);
}

static bool StackContains(Pthread thread, const void* address) {
    PthreadAttr attr = nullptr;
    if (scePthreadAttrInit(&attr) != 0 || scePthreadAttrGet(thread, &attr) != 0) return false;
    void* stack = nullptr;
    size_t size = 0;
    const bool queried = scePthreadAttrGetstackaddr(&attr, &stack) == 0 && scePthreadAttrGetstacksize(&attr, &size) == 0;
    scePthreadAttrDestroy(&attr);
    const auto begin = reinterpret_cast<std::uintptr_t>(stack);
    const auto value = reinterpret_cast<std::uintptr_t>(address);
    return queried && stack != nullptr && size != 0 && value >= begin && value - begin < size;
}

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EINVAL = 0x80020016;
static constexpr int SCE_KERNEL_ERROR_EPERM = 0x80020001;
static constexpr int MUTEX_TYPE_RECURSIVE = 2;
static constexpr int PTHREAD_CANCEL_ENABLE = 0;
static constexpr int PTHREAD_CANCEL_ASYNCHRONOUS = 2;
static constexpr std::intptr_t WorkerRetval = 0x1234;

static void Require(bool value) { if (!value) std::abort(); }

struct WorkerContext {
    Pthread thread = nullptr;
    Pthread selfFromWorker = nullptr;
    bool workerStackReported = false;
    PthreadMutex* mutex = nullptr;
    int unlockResult = 0;
    bool testcancelReturned = false;
};

static void* APS5_VABI Worker(void* arg) {
    auto& context = *static_cast<WorkerContext*>(arg);
    context.selfFromWorker = scePthreadSelf();
    int local = 0;
    context.workerStackReported = StackContains(context.selfFromWorker, &local);
    context.unlockResult = scePthreadMutexUnlock(context.mutex);
    int oldState = -1;
    int oldType = -1;
    context.testcancelReturned = scePthreadSetcancelstate(PTHREAD_CANCEL_ENABLE, &oldState) == SCE_OK &&
        scePthreadSetcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldType) == SCE_OK;
    scePthreadTestcancel();
    pthread_testcancel_nid_postfix();
    context.testcancelReturned = context.testcancelReturned && oldState == PTHREAD_CANCEL_ENABLE;
    scePthreadExit(reinterpret_cast<void*>(WorkerRetval));
    return nullptr;
}

int main() {
    const Pthread mainSelf = scePthreadSelf();
    Require(mainSelf != nullptr);
    Require(scePthreadSelf() == mainSelf);
    int local = 0;
    Require(StackContains(mainSelf, &local));
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
    Require(context.workerStackReported);
    Require(context.testcancelReturned);

    scePthreadTestcancel();
    pthread_testcancel_nid_postfix();

    Require(scePthreadMutexUnlock(&mutex) == SCE_OK);
    Require(scePthreadMutexDestroy(&mutex) == SCE_OK);
}
