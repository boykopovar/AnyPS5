#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "Testing.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <thread>

namespace {

constexpr int PosixEperm = 1;
constexpr int PosixEinval = 22;
constexpr int PosixEtimedout = 60;
constexpr int PosixEbusy = 16;

void TestTimedwaitPrecedence() {
    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);

    PthreadCond cond = nullptr;
    Require(pthread_cond_init_nid_postfix(&cond, nullptr) == 0);

    const KernelTimespec validPast{0, 0};
    Require(pthread_cond_timedwait_nid_postfix(nullptr, &mutex, &validPast) == PosixEinval);
    Require(pthread_cond_timedwait_nid_postfix(&cond, nullptr, &validPast) == PosixEinval);
    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, nullptr) == PosixEinval);
    Require(pthread_cond_timedwait_nid_postfix(nullptr, nullptr, nullptr) == PosixEinval);

    const KernelTimespec invalidTimespecs[] = {
        {-1, 0},
        {-100, 500},
        {0, -1},
        {10, -500},
        {0, 1000000000},
        {0, 1000000001},
        {10, 2000000000},
        {-5, -5},
        {-5, 1000000000}
    };

    for (const auto& ts : invalidTimespecs) {
        Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &ts) == PosixEinval);
    }

    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &validPast) == PosixEperm);

    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);

    for (const auto& ts : invalidTimespecs) {
        Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &ts) == PosixEinval);
    }

    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &validPast) == PosixEtimedout);
    Require(pthread_cond_timedwait_nid_postfix(&cond, &mutex, &validPast) == PosixEtimedout);

    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);

    Require(pthread_cond_destroy_nid_postfix(&cond) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
}

void TestCondDestroyAndMutexBusy() {
    Require(pthread_cond_destroy_nid_postfix(nullptr) == PosixEinval);

    PthreadCond cond = nullptr;
    Require(pthread_cond_destroy_nid_postfix(&cond) == 0);

    Require(pthread_cond_init_nid_postfix(&cond, nullptr) == 0);
    Require(pthread_cond_destroy_nid_postfix(&cond) == 0);
    Require(pthread_cond_destroy_nid_postfix(&cond) == PosixEinval);

    PthreadMutex mutex = nullptr;
    Require(pthread_mutex_init_nid_postfix(&mutex, nullptr) == 0);
    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == PosixEbusy);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
}

struct BroadcastState {
    PthreadMutex mutex = nullptr;
    PthreadCond cond = nullptr;
    std::atomic<int> waiting{0};
    std::atomic<int> woken{0};
    bool ready = false;
};

void* APS5_VABI WaiterWorker(void* arg) {
    auto* state = static_cast<BroadcastState*>(arg);
    Require(pthread_mutex_lock_nid_postfix(&state->mutex) == 0);
    state->waiting.fetch_add(1, std::memory_order_relaxed);
    while (!state->ready) {
        Require(pthread_cond_wait_nid_postfix(&state->cond, &state->mutex) == 0);
    }
    state->woken.fetch_add(1, std::memory_order_relaxed);
    Require(pthread_mutex_unlock_nid_postfix(&state->mutex) == 0);
    return nullptr;
}

void TestBroadcastSemantics() {
    Require(pthread_cond_broadcast_nid_postfix(nullptr) == PosixEinval);

    PthreadCond destroyed = nullptr;
    Require(pthread_cond_init_nid_postfix(&destroyed, nullptr) == 0);
    Require(pthread_cond_destroy_nid_postfix(&destroyed) == 0);
    Require(pthread_cond_broadcast_nid_postfix(&destroyed) == PosixEinval);

    PthreadCond fresh = nullptr;
    Require(pthread_cond_broadcast_nid_postfix(&fresh) == 0);
    Require(pthread_cond_destroy_nid_postfix(&fresh) == 0);

    BroadcastState state;
    Require(pthread_mutex_init_nid_postfix(&state.mutex, nullptr) == 0);
    Require(pthread_cond_init_nid_postfix(&state.cond, nullptr) == 0);

    Pthread thread1 = nullptr;
    Pthread thread2 = nullptr;
    Require(scePthreadCreate(&thread1, nullptr, WaiterWorker, &state, "waiter1") == 0);
    Require(scePthreadCreate(&thread2, nullptr, WaiterWorker, &state, "waiter2") == 0);

    while (state.waiting.load(std::memory_order_relaxed) < 2) {
        std::this_thread::yield();
    }

    Require(pthread_mutex_lock_nid_postfix(&state.mutex) == 0);
    state.ready = true;
    Require(pthread_cond_broadcast_nid_postfix(&state.cond) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&state.mutex) == 0);

    void* retVal = nullptr;
    Require(scePthreadJoin(thread1, &retVal) == 0);
    Require(scePthreadJoin(thread2, &retVal) == 0);

    Require(state.woken.load(std::memory_order_relaxed) == 2);

    Require(pthread_cond_destroy_nid_postfix(&state.cond) == 0);
    Require(pthread_mutex_destroy_nid_postfix(&state.mutex) == 0);
}

} // namespace

int main() {
    TestTimedwaitPrecedence();
    TestCondDestroyAndMutexBusy();
    TestBroadcastSemantics();
    return 0;
}
