#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace {

constexpr int Ebusy = 16;
constexpr int TypeErrorCheck = 1;
constexpr int TypeRecursive = 2;
constexpr int TypeNormal = 3;

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

void WaitFor(const std::atomic<bool>& flag, const char* message) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!flag && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Require(flag, message);
}

PthreadMutex Make(int type) {
    PthreadMutexattr attr = nullptr;
    PthreadMutex mutex = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0, "mutexattr init");
    Require(pthread_mutexattr_settype_nid_postfix(&attr, type) == 0, "mutexattr settype");
    Require(pthread_mutex_init_nid_postfix(&mutex, &attr) == 0, "mutex init");
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0, "mutexattr destroy");
    return mutex;
}

}

int main() {
    PthreadMutex fresh = nullptr;
    Require(pthread_mutex_trylock_nid_postfix(&fresh) == 0, "trylock of a statically initialised mutex returns 0");
    Require(pthread_mutex_trylock_nid_postfix(&fresh) == Ebusy, "trylock of a held statically initialised mutex returns EBUSY");
    Require(pthread_mutex_unlock_nid_postfix(&fresh) == 0, "unlock after trylock returns 0");
    Require(pthread_mutex_destroy_nid_postfix(&fresh) == 0, "destroy returns 0");

    for (const int type : {TypeErrorCheck, TypeNormal}) {
        PthreadMutex mutex = Make(type);
        Require(pthread_mutex_trylock_nid_postfix(&mutex) == 0, "trylock of a free mutex returns 0");
        Require(pthread_mutex_trylock_nid_postfix(&mutex) == Ebusy, "trylock of a mutex held by the caller returns EBUSY");
        Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0, "unlock returns 0");
        Require(pthread_mutex_trylock_nid_postfix(&mutex) == 0, "trylock after unlock returns 0");
        Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0, "second unlock returns 0");
        Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0, "destroy returns 0");
    }

    PthreadMutex recursive = Make(TypeRecursive);
    Require(pthread_mutex_trylock_nid_postfix(&recursive) == 0, "recursive trylock returns 0");
    Require(pthread_mutex_trylock_nid_postfix(&recursive) == 0, "recursive trylock by the holder returns 0 again");
    Require(pthread_mutex_unlock_nid_postfix(&recursive) == 0, "first recursive unlock returns 0");
    Require(pthread_mutex_unlock_nid_postfix(&recursive) == 0, "second recursive unlock returns 0");
    Require(pthread_mutex_destroy_nid_postfix(&recursive) == 0, "destroy returns 0");

    PthreadMutex shared = Make(TypeNormal);
    std::atomic<bool> held{false};
    std::atomic<bool> release{false};
    std::atomic<bool> finished{false};
    std::thread holder([&] {
        Require(pthread_mutex_lock_nid_postfix(&shared) == 0, "helper lock returns 0");
        held = true;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!release && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        Require(pthread_mutex_unlock_nid_postfix(&shared) == 0, "helper unlock returns 0");
        finished = true;
    });
    WaitFor(held, "helper never took the mutex");
    Require(pthread_mutex_trylock_nid_postfix(&shared) == Ebusy, "trylock of a mutex held by another thread returns EBUSY");
    release = true;
    WaitFor(finished, "helper never released the mutex");
    holder.join();
    Require(pthread_mutex_trylock_nid_postfix(&shared) == 0, "trylock after the other thread released returns 0");
    Require(pthread_mutex_unlock_nid_postfix(&shared) == 0, "unlock returns 0");
    Require(pthread_mutex_destroy_nid_postfix(&shared) == 0, "destroy returns 0");
    return 0;
}
