#include "SceTypes.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

extern "C" {
int APS5_VABI pthread_mutex_init_nid_postfix(PthreadMutex*, const PthreadMutexattr*);
int APS5_VABI pthread_mutex_destroy_nid_postfix(PthreadMutex*);
int APS5_VABI pthread_mutex_lock_nid_postfix(PthreadMutex*);
int APS5_VABI pthread_mutex_unlock_nid_postfix(PthreadMutex*);
int APS5_VABI pthread_mutex_setname_np_nid_postfix(PthreadMutex*, const char*);
int APS5_VABI pthread_rwlock_init_nid_postfix(PthreadRwlock*, const PthreadRwlockattr*);
int APS5_VABI pthread_rwlock_destroy_nid_postfix(PthreadRwlock*);
int APS5_VABI pthread_rwlock_rdlock_nid_postfix(PthreadRwlock*);
int APS5_VABI pthread_rwlock_unlock_nid_postfix(PthreadRwlock*);
int APS5_VABI pthread_rwlock_setname_np_nid_postfix(PthreadRwlock*, const char*);
int APS5_VABI pthread_barrier_init_nid_postfix(void**, void* const*, unsigned);
int APS5_VABI pthread_barrier_destroy_nid_postfix(void**);
int APS5_VABI pthread_barrier_wait_nid_postfix(void**);
int APS5_VABI pthread_barrier_setname_np_nid_postfix(void**, const char*);
}

static constexpr int GuestEinval = 22;

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "pthread setname check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static void TestMutex() {
    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);
    Require(pthread_mutex_setname_np_nid_postfix(&mutex, "first") == 0 && mutex->_name == "first");
    Require(pthread_mutex_setname_np_nid_postfix(&mutex, "second") == 0 && mutex->_name == "second");
    Require(pthread_mutex_setname_np_nid_postfix(&mutex, nullptr) == 0 && mutex->_name == "second");
    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_setname_np_nid_postfix(&mutex, "held") == 0 && mutex->_name == "held");
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    RequireThrows([] { pthread_mutex_setname_np_nid_postfix(nullptr, "mutex"); });
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    RequireThrows([&] { pthread_mutex_setname_np_nid_postfix(&mutex, "destroyed"); });

    PthreadMutex unset = nullptr;
    RequireThrows([&] { pthread_mutex_setname_np_nid_postfix(&unset, "static"); });
    Require(unset == nullptr);
    PthreadMutex adaptive = reinterpret_cast<PthreadMutex>(std::uintptr_t{1});
    RequireThrows([&] { pthread_mutex_setname_np_nid_postfix(&adaptive, "adaptive"); });
    Require(reinterpret_cast<std::uintptr_t>(adaptive) == 1);
}

static void TestRwlock() {
    PthreadRwlock rwlock = nullptr;
    Require(pthread_rwlock_init_nid_postfix(&rwlock, nullptr) == 0);
    Require(pthread_rwlock_setname_np_nid_postfix(&rwlock, "first") == 0 && rwlock->_name == "first");
    Require(pthread_rwlock_setname_np_nid_postfix(&rwlock, nullptr) == GuestEinval && rwlock->_name == "first");
    Require(pthread_rwlock_rdlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_setname_np_nid_postfix(&rwlock, "read") == 0 && rwlock->_name == "read");
    Require(pthread_rwlock_unlock_nid_postfix(&rwlock) == 0);
    Require(pthread_rwlock_setname_np_nid_postfix(nullptr, nullptr) == GuestEinval);
    RequireThrows([] { pthread_rwlock_setname_np_nid_postfix(nullptr, "rwlock"); });
    Require(pthread_rwlock_destroy_nid_postfix(&rwlock) == 0);
    RequireThrows([&] { pthread_rwlock_setname_np_nid_postfix(&rwlock, "destroyed"); });

    PthreadRwlock unset = nullptr;
    Require(pthread_rwlock_setname_np_nid_postfix(&unset, "static") == 0 && unset == nullptr);
}

static void TestBarrier() {
    void* barrier = nullptr;
    Require(pthread_barrier_init_nid_postfix(&barrier, nullptr, 1) == 0);
    Require(pthread_barrier_setname_np_nid_postfix(&barrier, "first") == 0);
    Require(pthread_barrier_setname_np_nid_postfix(&barrier, nullptr) == 0);
    Require(pthread_barrier_wait_nid_postfix(&barrier) == -1);
    Require(pthread_barrier_setname_np_nid_postfix(&barrier, "second") == 0);
    Require(pthread_barrier_setname_np_nid_postfix(nullptr, "barrier") == GuestEinval);
    Require(pthread_barrier_destroy_nid_postfix(&barrier) == 0);
    Require(pthread_barrier_setname_np_nid_postfix(&barrier, "destroyed") == GuestEinval);
    void* unset = nullptr;
    Require(pthread_barrier_setname_np_nid_postfix(&unset, "unset") == GuestEinval && unset == nullptr);
}

int main() {
    TestMutex();
    TestRwlock();
    TestBarrier();
    std::puts("pthread setname tests passed");
}
