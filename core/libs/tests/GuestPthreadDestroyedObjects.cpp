#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>

static constexpr int SCE_OK = 0;
static constexpr int POSIX_EINVAL = 22;

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    PthreadMutex mutex = nullptr;
    Require(scePthreadMutexInit(&mutex, nullptr, "destroyed") == SCE_OK);
    Require(scePthreadMutexDestroy(&mutex) == SCE_OK);
    Require(scePthreadMutexLock(&mutex) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexTrylock(&mutex) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadMutexUnlock(&mutex) == SCE_KERNEL_ERROR_EINVAL);
    Require(pthread_mutex_lock_nid_postfix(&mutex) == POSIX_EINVAL);
    Require(mutex == reinterpret_cast<PthreadMutex>(std::uintptr_t{2}));

    PthreadMutex uninitialized = nullptr;
    Require(scePthreadMutexUnlock(&uninitialized) == SCE_KERNEL_ERROR_EPERM);
    Require(uninitialized == nullptr);
    Require(scePthreadMutexLock(&uninitialized) == SCE_OK);
    Require(scePthreadMutexUnlock(&uninitialized) == SCE_OK);
    Require(scePthreadMutexUnlock(&uninitialized) == SCE_KERNEL_ERROR_EPERM);
    Require(scePthreadMutexDestroy(&uninitialized) == SCE_OK);

    PthreadMutex adaptive = reinterpret_cast<PthreadMutex>(std::uintptr_t{1});
    Require(scePthreadMutexUnlock(&adaptive) == SCE_KERNEL_ERROR_EPERM);
    Require(adaptive == reinterpret_cast<PthreadMutex>(std::uintptr_t{1}));
    Require(scePthreadMutexLock(&adaptive) == SCE_OK);
    Require(scePthreadMutexUnlock(&adaptive) == SCE_OK);
    Require(scePthreadMutexDestroy(&adaptive) == SCE_OK);

    PthreadCond cond = nullptr;
    Require(scePthreadCondInit(&cond, nullptr, "destroyed") == SCE_OK);
    Require(scePthreadCondDestroy(&cond) == SCE_OK);
    Require(reinterpret_cast<std::uintptr_t>(cond) == 1);
    Require(scePthreadCondSignal(&cond) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadCondBroadcast(&cond) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadCondWait(&cond, &mutex) == SCE_KERNEL_ERROR_EINVAL);
    Require(pthread_cond_signal_nid_postfix(&cond) == POSIX_EINVAL);
    Require(scePthreadCondDestroy(&cond) == SCE_KERNEL_ERROR_EINVAL);
    Require(pthread_cond_destroy_nid_postfix(&cond) == POSIX_EINVAL);

    PthreadCond untouched = nullptr;
    Require(scePthreadCondDestroy(&untouched) == SCE_OK && untouched == nullptr);
    Require(scePthreadCondSignal(&untouched) == SCE_OK && untouched != nullptr);
    Require(scePthreadCondDestroy(&untouched) == SCE_OK && reinterpret_cast<std::uintptr_t>(untouched) == 1);
    untouched = nullptr;
    Require(scePthreadCondBroadcast(&untouched) == SCE_OK && untouched != nullptr);
    Require(scePthreadCondDestroy(&untouched) == SCE_OK);
}
