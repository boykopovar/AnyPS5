#include "SceTypes.hpp"
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <thread>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI scePthreadMutexInit(PthreadMutex* mutex, const PthreadMutexattr* attr, const char* name);
int APS5_VABI scePthreadMutexDestroy(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexLock(PthreadMutex* mutex);
int APS5_VABI scePthreadMutexUnlock(PthreadMutex* mutex);
int APS5_VABI scePthreadCondInit(PthreadCond* cond, const PthreadCondattr* attr, const char* name);
int APS5_VABI scePthreadCondDestroy(PthreadCond* cond);
int APS5_VABI scePthreadCondWait(PthreadCond* cond, PthreadMutex* mutex);
int APS5_VABI scePthreadCondTimedwait(PthreadCond* cond, PthreadMutex* mutex, unsigned int usec);
int APS5_VABI pthread_cond_wait_nid_postfix(PthreadCond* cond, PthreadMutex* mutex);
int APS5_VABI pthread_cond_timedwait_nid_postfix(PthreadCond* cond, PthreadMutex* mutex, const KernelTimespec* abstime);
int APS5_VABI clock_gettime_nid_postfix(int clockId, KernelTimespec* tp);
}

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EPERM = static_cast<int>(0x80020001);
static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);
static constexpr int POSIX_EPERM = 1;
static constexpr int POSIX_EINVAL = 22;
static constexpr std::uintptr_t ADAPTIVE_INITIALIZER = 1;

static void Require(bool value) { if (!value) std::abort(); }

static int PosixTimed(PthreadCond* cond, PthreadMutex* mutex) {
    KernelTimespec deadline{};
    Require(clock_gettime_nid_postfix(0, &deadline) == 0);
    deadline.tv_sec += 60;
    return pthread_cond_timedwait_nid_postfix(cond, mutex, &deadline);
}

struct Wait {
    int (*run)(PthreadCond*, PthreadMutex*);
    int notOwned;
    int destroyed;
};

static int SceWait(PthreadCond* cond, PthreadMutex* mutex) { return scePthreadCondWait(cond, mutex); }
static int SceTimed(PthreadCond* cond, PthreadMutex* mutex) { return scePthreadCondTimedwait(cond, mutex, 60000000); }
static int PosixWait(PthreadCond* cond, PthreadMutex* mutex) { return pthread_cond_wait_nid_postfix(cond, mutex); }

struct Holder {
    PthreadMutex mutex = nullptr;
    std::atomic<bool> locked{false};
    std::atomic<bool> release{false};
};

static void* APS5_VABI Hold(void* arg) {
    auto& holder = *static_cast<Holder*>(arg);
    Require(scePthreadMutexLock(&holder.mutex) == SCE_OK);
    holder.locked.store(true);
    while (!holder.release.load())
        std::this_thread::yield();
    Require(scePthreadMutexUnlock(&holder.mutex) == SCE_OK);
    return nullptr;
}

static void Check(const Wait& wait) {
    PthreadCond cond = nullptr;
    Require(scePthreadCondInit(&cond, nullptr, nullptr) == SCE_OK);

    PthreadMutex unlocked = nullptr;
    Require(scePthreadMutexInit(&unlocked, nullptr, nullptr) == SCE_OK);
    Require(wait.run(&cond, &unlocked) == wait.notOwned);
    Require(scePthreadMutexLock(&unlocked) == SCE_OK);
    Require(scePthreadMutexUnlock(&unlocked) == SCE_OK);
    Require(scePthreadMutexDestroy(&unlocked) == SCE_OK);

    PthreadMutex uninitialized = nullptr;
    Require(wait.run(&cond, &uninitialized) == wait.notOwned);

    PthreadMutex adaptive = reinterpret_cast<PthreadMutex>(ADAPTIVE_INITIALIZER);
    Require(wait.run(&cond, &adaptive) == wait.notOwned);

    PthreadMutex destroyed = nullptr;
    Require(scePthreadMutexInit(&destroyed, nullptr, nullptr) == SCE_OK);
    Require(scePthreadMutexDestroy(&destroyed) == SCE_OK);
    Require(wait.run(&cond, &destroyed) == wait.destroyed);

    Holder holder;
    Require(scePthreadMutexInit(&holder.mutex, nullptr, nullptr) == SCE_OK);
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Hold, &holder, nullptr) == SCE_OK);
    while (!holder.locked.load())
        std::this_thread::yield();
    Require(wait.run(&cond, &holder.mutex) == wait.notOwned);
    holder.release.store(true);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    Require(scePthreadMutexDestroy(&holder.mutex) == SCE_OK);

    Require(scePthreadCondDestroy(&cond) == SCE_OK);
}

int main() {
    Check({SceWait, SCE_KERNEL_ERROR_EPERM, SCE_KERNEL_ERROR_EINVAL});
    Check({SceTimed, SCE_KERNEL_ERROR_EPERM, SCE_KERNEL_ERROR_EINVAL});
    Check({PosixWait, POSIX_EPERM, POSIX_EINVAL});
    Check({PosixTimed, POSIX_EPERM, POSIX_EINVAL});
}
