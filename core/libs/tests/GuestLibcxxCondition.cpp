#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/LibcxxConditionForwards.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <system_error>
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

std::atomic<int> forcedForwardError{22};

int APS5_VABI FailingCondOp(PthreadCond*) {
    return forcedForwardError.load(std::memory_order_acquire);
}

int APS5_VABI FailingCondTimedwait(PthreadCond*, PthreadMutex*, const KernelTimespec*) {
    return forcedForwardError.load(std::memory_order_acquire);
}

int APS5_VABI FailingCondWait(PthreadCond*, PthreadMutex*) {
    return forcedForwardError.load(std::memory_order_acquire);
}

struct LibcxxConditionForwardsGuard {
    const LibcxxConditionForwards_nid_no_patch* restore;
    explicit LibcxxConditionForwardsGuard(const LibcxxConditionForwards_nid_no_patch* fallback) : restore(fallback) {}
    ~LibcxxConditionForwardsGuard() { RegisterLibcxxConditionForwards_nid_no_patch(restore); }
    LibcxxConditionForwardsGuard(const LibcxxConditionForwardsGuard&) = delete;
    LibcxxConditionForwardsGuard& operator=(const LibcxxConditionForwardsGuard&) = delete;
};

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
    Require(pthread_cond_signal_nid_postfix(&condition) == 22, "destructor did not destroy condition handle");

    bool destroyedOperationsThrew = false;
    try {
        _ZNSt3__118condition_variable10notify_oneEv_nid_postfix(&condition);
        _ZNSt3__118condition_variable10notify_allEv_nid_postfix(&condition);
        _ZNSt3__118condition_variableD1Ev_nid_postfix(&condition);
    } catch (...) {
        destroyedOperationsThrew = true;
    }
    Require(!destroyedOperationsThrew, "noexcept condition_variable operations threw on a destroyed handle");

    PthreadCond uninitialized = nullptr;
    PthreadCond unused = nullptr;
    bool defaultInitializedThrew = false;
    try {
        _ZNSt3__118condition_variable10notify_oneEv_nid_postfix(&uninitialized);
        _ZNSt3__118condition_variable10notify_allEv_nid_postfix(&uninitialized);
        _ZNSt3__118condition_variableD1Ev_nid_postfix(&uninitialized);
        _ZNSt3__118condition_variableD1Ev_nid_postfix(&unused);
    } catch (...) {
        defaultInitializedThrew = true;
    }
    Require(!defaultInitializedThrew, "noexcept condition_variable operations threw on a default-initialized handle");

    static const LibcxxConditionForwards_nid_no_patch libkernelForwards{
        pthread_cond_destroy_nid_postfix,
        pthread_cond_broadcast_nid_postfix,
        pthread_cond_signal_nid_postfix,
        pthread_cond_timedwait_nid_postfix,
        pthread_cond_wait_nid_postfix,
    };
    static const LibcxxConditionForwards_nid_no_patch failingForwards{
        FailingCondOp,
        FailingCondOp,
        FailingCondOp,
        FailingCondTimedwait,
        FailingCondWait,
    };
    {
        LibcxxConditionForwardsGuard forwardsGuard(&libkernelForwards);
        RegisterLibcxxConditionForwards_nid_no_patch(&failingForwards);
        PthreadCond mocked = nullptr;
        for (const int code : {22, 16}) {
            forcedForwardError.store(code, std::memory_order_release);
            bool noexceptThrew = false;
            try {
                _ZNSt3__118condition_variable10notify_oneEv_nid_postfix(&mocked);
                _ZNSt3__118condition_variable10notify_allEv_nid_postfix(&mocked);
                _ZNSt3__118condition_variableD1Ev_nid_postfix(&mocked);
            } catch (...) {
                noexceptThrew = true;
            }
            Require(!noexceptThrew, "noexcept condition_variable methods threw on failing libkernel forwards");

            Require(pthread_mutex_lock_nid_postfix(&mutex) == 0, "failing forward lock failed");
            lock.owns = true;
            int waitError = 0;
            try {
                _ZNSt3__118condition_variable4waitERNS_11unique_lockINS_5mutexEEE_nid_postfix(&mocked, &lock);
            } catch (const std::system_error& error) {
                waitError = error.code().value();
            }
            Require(waitError == code, "wait did not propagate failing forward error as system_error");

            int timedWaitError = 0;
            try {
                _ZNSt3__118condition_variable15__do_timed_waitERNS_11unique_lockINS_5mutexEEENS_6chrono10time_pointINS5_12system_clockENS5_8durationIxNS_5ratioILl1ELl1000000000EEEEEEE_nid_postfix(&mocked, &lock, past);
            } catch (const std::system_error& error) {
                timedWaitError = error.code().value();
            }
            Require(timedWaitError == code, "__do_timed_wait did not propagate failing forward error as system_error");
            Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0, "failing forward unlock failed");
        }
        bool notifyOneRejected = false;
        try {
            _ZNSt3__118condition_variable10notify_oneEv_nid_postfix(nullptr);
        } catch (const std::invalid_argument&) {
            notifyOneRejected = true;
        }
        Require(notifyOneRejected, "notify_one accepted a null condition variable pointer");

        bool notifyAllRejected = false;
        try {
            _ZNSt3__118condition_variable10notify_allEv_nid_postfix(nullptr);
        } catch (const std::invalid_argument&) {
            notifyAllRejected = true;
        }
        Require(notifyAllRejected, "notify_all accepted a null condition variable pointer");

        bool destroyRejected = false;
        try {
            _ZNSt3__118condition_variableD1Ev_nid_postfix(nullptr);
        } catch (const std::invalid_argument&) {
            destroyRejected = true;
        }
        Require(destroyRejected, "destructor accepted a null condition variable pointer");
    }

    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0, "pthread mutex destroy failed");
    return 0;
}
