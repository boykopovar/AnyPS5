#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include <cerrno>
#include <cstdlib>
#include <thread>
#include <atomic>

extern "C" int APS5_VABI pthread_cond_init_nid_postfix(PthreadCond*, const PthreadCondattr*);
extern "C" int APS5_VABI pthread_cond_destroy_nid_postfix(PthreadCond*);
extern "C" int APS5_VABI scePthreadCondInit(PthreadCond*, const PthreadCondattr*, const char*);
extern "C" int APS5_VABI scePthreadCondDestroy(PthreadCond*);
extern "C" int APS5_VABI scePthreadCondTimedwait(PthreadCond*, PthreadMutex*, unsigned int);
extern "C" int APS5_VABI pthread_cond_wait_nid_postfix(PthreadCond*, PthreadMutex*);
extern "C" int APS5_VABI pthread_cond_signal_nid_postfix(PthreadCond*);
extern "C" int APS5_VABI pthread_cond_broadcast_nid_postfix(PthreadCond*);
extern "C" int APS5_VABI scePthreadMutexInit(PthreadMutex*, const PthreadMutexattr*, const char*);
extern "C" int APS5_VABI scePthreadMutexDestroy(PthreadMutex*);
extern "C" int APS5_VABI scePthreadMutexLock(PthreadMutex*);
extern "C" int APS5_VABI scePthreadMutexUnlock(PthreadMutex*);

static void Require(bool condition) { if (!condition) std::abort(); }

int main() {
    PthreadCond cond = nullptr;
    Require(pthread_cond_init_nid_postfix(nullptr, nullptr) == EINVAL);
    Require(pthread_cond_init_nid_postfix(&cond, nullptr) == 0 && cond);
    Require(pthread_cond_destroy_nid_postfix(&cond) == 0 && !cond);
    Require(pthread_cond_destroy_nid_postfix(&cond) == EINVAL);
    Require(pthread_cond_destroy_nid_postfix(nullptr) == EINVAL);

    PthreadCondattr invalid = nullptr;
    Require(pthread_cond_init_nid_postfix(&cond, &invalid) == EINVAL && !cond);
    Require(scePthreadCondInit(&cond, nullptr, nullptr) == 0 && cond);
    Require(pthread_cond_destroy_nid_postfix(&cond) == 0 && !cond);
    Require(pthread_cond_init_nid_postfix(&cond, nullptr) == 0 && cond);
    Require(scePthreadCondDestroy(&cond) == 0);

    PthreadMutex mutex = nullptr;
    Require(scePthreadMutexInit(&mutex, nullptr, nullptr) == 0);
    Require(pthread_cond_init_nid_postfix(&cond, nullptr) == 0);
    Require(pthread_cond_signal_nid_postfix(nullptr) == EINVAL);
    Require(pthread_cond_broadcast_nid_postfix(nullptr) == EINVAL);
    Require(pthread_cond_wait_nid_postfix(&cond, &mutex) == EPERM);
    std::atomic<bool> ready{false};
    bool published = false;
    std::thread notifier([&] {
        while (!ready.load(std::memory_order_acquire)) std::this_thread::yield();
        Require(scePthreadMutexLock(&mutex) == 0);
        Require(pthread_cond_destroy_nid_postfix(&cond) == EBUSY);
        published = true;
        Require(pthread_cond_signal_nid_postfix(&cond) == 0);
        Require(scePthreadMutexUnlock(&mutex) == 0);
    });
    Require(scePthreadMutexLock(&mutex) == 0);
    ready.store(true, std::memory_order_release);
    while (!published) Require(pthread_cond_wait_nid_postfix(&cond, &mutex) == 0);
    Require(scePthreadMutexUnlock(&mutex) == 0);
    notifier.join();
    Require(pthread_cond_broadcast_nid_postfix(&cond) == 0);
    Require(scePthreadMutexLock(&mutex) == 0);
    Require(static_cast<unsigned>(scePthreadCondTimedwait(&cond, &mutex, 1000)) == 0x8002003cu);
    Require(scePthreadMutexUnlock(&mutex) == 0);
    Require(pthread_cond_destroy_nid_postfix(&cond) == 0);
    Require(scePthreadMutexDestroy(&mutex) == 0);
}
