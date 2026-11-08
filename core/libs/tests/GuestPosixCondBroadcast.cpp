#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

namespace {

constexpr int Waiters = 3;
constexpr int Einval = 22;
constexpr int Etimedout = 60;

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

struct State {
    PthreadMutex mutex = nullptr;
    PthreadCond cond = nullptr;
    int waiting = 0;
    bool go = false;
    std::atomic<int> woken{0};
    std::atomic<int> failed{0};
};

void Wait(State& state, std::chrono::steady_clock::time_point deadline) {
    Require(pthread_mutex_lock_nid_postfix(&state.mutex) == 0, "waiter lock returns 0");
    ++state.waiting;
    int result = 0;
    while (!state.go && result == 0) {
        const auto left = std::chrono::duration_cast<std::chrono::microseconds>(deadline - std::chrono::steady_clock::now()).count();
        result = left > 0 ? pthread_cond_reltimedwait_np_nid_postfix(&state.cond, &state.mutex, static_cast<KernelUseconds>(left)) : Etimedout;
    }
    if (state.go && result == 0) ++state.woken;
    else ++state.failed;
    Require(pthread_mutex_unlock_nid_postfix(&state.mutex) == 0, "waiter unlock returns 0");
}

}

int main() {
    PthreadCond idle = nullptr;
    Require(pthread_cond_init_nid_postfix(&idle, nullptr) == 0, "cond init");
    Require(pthread_cond_broadcast_nid_postfix(&idle) == 0, "broadcast without waiters returns 0");
    Require(pthread_cond_destroy_nid_postfix(&idle) == 0, "destroy returns 0");
    Require(pthread_cond_broadcast_nid_postfix(&idle) == Einval, "broadcast of a destroyed cond returns EINVAL");

    PthreadCond fresh = nullptr;
    Require(pthread_cond_broadcast_nid_postfix(&fresh) == 0, "broadcast of a statically initialised cond returns 0");
    Require(fresh != nullptr, "broadcast of a statically initialised cond initialises it");
    Require(pthread_cond_destroy_nid_postfix(&fresh) == 0, "destroy of the initialised cond returns 0");

    State state;
    Require(pthread_mutex_init_nid_postfix(&state.mutex, nullptr) == 0, "mutex init");
    Require(pthread_cond_init_nid_postfix(&state.cond, nullptr) == 0, "cond init");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    std::vector<std::thread> threads;
    for (int i = 0; i < Waiters; ++i) threads.emplace_back([&] { Wait(state, deadline); });

    while (std::chrono::steady_clock::now() < deadline) {
        Require(pthread_mutex_lock_nid_postfix(&state.mutex) == 0, "lock returns 0");
        const bool all = state.waiting == Waiters;
        Require(pthread_mutex_unlock_nid_postfix(&state.mutex) == 0, "unlock returns 0");
        if (all) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(pthread_mutex_lock_nid_postfix(&state.mutex) == 0, "lock returns 0");
    Require(state.waiting == Waiters, "all waiters started waiting");
    state.go = true;
    Require(pthread_cond_broadcast_nid_postfix(&state.cond) == 0, "broadcast with waiters returns 0");
    Require(pthread_mutex_unlock_nid_postfix(&state.mutex) == 0, "unlock returns 0");
    for (auto& thread : threads) thread.join();
    std::fprintf(stderr, "woken %d of %d\n", state.woken.load(), Waiters);
    Require(state.woken == Waiters && state.failed == 0, "one broadcast wakes every waiter");
    Require(pthread_cond_destroy_nid_postfix(&state.cond) == 0, "destroy returns 0");
    Require(pthread_mutex_destroy_nid_postfix(&state.mutex) == 0, "mutex destroy returns 0");
    return 0;
}
