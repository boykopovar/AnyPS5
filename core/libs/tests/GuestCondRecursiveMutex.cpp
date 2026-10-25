#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "SceTypes.hpp"
#include <cstdlib>

static constexpr int SCE_OK = 0;
static constexpr int POSIX_ETIMEDOUT = 60;
static constexpr int MUTEX_TYPE_RECURSIVE = 2;

static void Require(bool value) { if (!value) std::abort(); }

struct Context {
    PthreadMutex mutex = nullptr;
    PthreadCond cond = nullptr;
    bool signalled = false;
};

static void* APS5_VABI Signaller(void* arg) {
    auto& context = *static_cast<Context*>(arg);
    Require(scePthreadMutexLock(&context.mutex) == SCE_OK);
    context.signalled = true;
    Require(scePthreadCondSignal(&context.cond) == SCE_OK);
    Require(scePthreadMutexUnlock(&context.mutex) == SCE_OK);
    return nullptr;
}

static int SceWait(Context& context) {
    return scePthreadCondWait(&context.cond, &context.mutex);
}

static int SceTimedwait(Context& context) {
    const int result = scePthreadCondTimedwait(&context.cond, &context.mutex, 1000);
    return result == SCE_KERNEL_ERROR_ETIMEDOUT ? SCE_OK : result;
}

static int PosixWait(Context& context) {
    return pthread_cond_wait_nid_postfix(&context.cond, &context.mutex);
}

static int PosixTimedwait(Context& context) {
    KernelTimespec deadline{};
    Require(clock_gettime_nid_postfix(0, &deadline) == 0);
    deadline.tv_nsec += 1000000;
    if (deadline.tv_nsec >= 1000000000) {
        deadline.tv_nsec -= 1000000000;
        ++deadline.tv_sec;
    }
    const int result = pthread_cond_timedwait_nid_postfix(&context.cond, &context.mutex, &deadline);
    return result == POSIX_ETIMEDOUT ? SCE_OK : result;
}

static void WaitWithRecursiveMutex(int (*wait)(Context&)) {
    Context context;
    PthreadMutexattr attr = nullptr;
    Require(scePthreadMutexattrInit(&attr) == SCE_OK);
    Require(scePthreadMutexattrSettype(&attr, MUTEX_TYPE_RECURSIVE) == SCE_OK);
    Require(scePthreadMutexInit(&context.mutex, &attr, nullptr) == SCE_OK);
    Require(scePthreadMutexattrDestroy(&attr) == SCE_OK);
    Require(scePthreadCondInit(&context.cond, nullptr, nullptr) == SCE_OK);

    Require(scePthreadMutexLock(&context.mutex) == SCE_OK);
    Require(scePthreadMutexLock(&context.mutex) == SCE_OK);
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Signaller, &context, nullptr) == SCE_OK);
    while (!context.signalled)
        Require(wait(context) == SCE_OK);
    Require(scePthreadMutexUnlock(&context.mutex) == SCE_OK);
    Require(scePthreadMutexUnlock(&context.mutex) == SCE_OK);
    Require(scePthreadMutexUnlock(&context.mutex) == SCE_KERNEL_ERROR_EPERM);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);

    Require(scePthreadCondDestroy(&context.cond) == SCE_OK);
    Require(scePthreadMutexDestroy(&context.mutex) == SCE_OK);
}

int main() {
    WaitWithRecursiveMutex(SceWait);
    WaitWithRecursiveMutex(SceTimedwait);
    WaitWithRecursiveMutex(PosixWait);
    WaitWithRecursiveMutex(PosixTimedwait);
}
