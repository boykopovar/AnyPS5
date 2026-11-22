#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <thread>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct GuestUniqueLock {
    PthreadMutex* mutex;
    bool owns;
    std::uint8_t padding[7]{};
};
static_assert(sizeof(GuestUniqueLock) == 16);

}

int main() {
    static_assert(sizeof(PthreadCond) == sizeof(std::uint64_t));
    PthreadMutex mutex = nullptr;
    PthreadCond condition = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0, "pthread mutex init failed");
    GuestUniqueLock lock{&mutex, true};

    std::atomic<int> waiting{0};
    bool ready = false;
    auto waiter = [&] {
        Require(pthread_mutex_lock_nid_postfix(&mutex) == 0, "waiter lock failed");
        ++waiting;
        while (!ready) _ZNSt3__118condition_variable4waitERNS_11unique_lockINS_5mutexEEE_nid_postfix(&condition, &lock);
        Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0, "waiter unlock failed");
    };
    std::thread first(waiter);
    std::thread second(waiter);
    while (waiting.load() != 2) std::this_thread::yield();
    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0, "main lock failed");
    ready = true;
    _ZNSt3__118condition_variable10notify_allEv_nid_postfix(&condition);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0, "main unlock failed");
    first.join();
    second.join();
    Require(condition != nullptr, "condition was not initialized lazily");

    ready = false;
    std::thread third(waiter);
    while (waiting.load() != 3) std::this_thread::yield();
    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0, "signal lock failed");
    ready = true;
    _ZNSt3__118condition_variable10notify_oneEv_nid_postfix(&condition);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0, "signal unlock failed");
    third.join();
    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0, "timed lock failed");
    const auto past = std::chrono::duration_cast<std::chrono::nanoseconds>(
        (std::chrono::system_clock::now() - std::chrono::seconds(1)).time_since_epoch()).count();
    _ZNSt3__118condition_variable15__do_timed_waitERNS_11unique_lockINS_5mutexEEENS_6chrono10time_pointINS5_12system_clockENS5_8durationIxNS_5ratioILl1ELl1000000000EEEEEEE_nid_postfix(&condition, &lock, past);
    for (const auto beforeEpoch : {std::int64_t{-1}, std::numeric_limits<std::int64_t>::min()})
        _ZNSt3__118condition_variable15__do_timed_waitERNS_11unique_lockINS_5mutexEEENS_6chrono10time_pointINS5_12system_clockENS5_8durationIxNS_5ratioILl1ELl1000000000EEEEEEE_nid_postfix(&condition, &lock, beforeEpoch);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0, "timed wait did not retain mutex ownership");

    lock.owns = false;
    bool rejected = false;
    try {
        _ZNSt3__118condition_variable4waitERNS_11unique_lockINS_5mutexEEE_nid_postfix(&condition, &lock);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected, "wait accepted a unique_lock without ownership");

    _ZNSt3__118condition_variableD1Ev_nid_postfix(&condition);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0, "pthread mutex destroy failed");
    return 0;
}
