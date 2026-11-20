#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <system_error>
#include <thread>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(1);
    }
}

struct WorkerState {
    std::atomic<bool> release{false};
    std::atomic<bool> finished{false};
};

void* APS5_VABI Worker(void* opaque) {
    auto* state = static_cast<WorkerState*>(opaque);
    while (!state->release.load()) std::this_thread::yield();
    state->finished.store(true);
    return nullptr;
}

void CheckEmpty() {
    Pthread empty = nullptr;
    for (const auto call : {_ZNSt3__16thread4joinEv_nid_postfix, _ZNSt3__16thread6detachEv_nid_postfix}) {
        bool rejected = false;
        try {
            call(&empty);
        } catch (const std::system_error& error) {
            rejected = error.code().value() == EINVAL;
        }
        Require(rejected && empty == nullptr, "empty thread must report EINVAL");
    }
    _ZNSt3__16threadD1Ev_nid_postfix(&empty);
}

void CheckJoin() {
    Pthread self = pthread_self_nid_postfix();
    Require(self != nullptr, "find current guest thread");
    bool selfJoinRejected = false;
    try {
        _ZNSt3__16thread4joinEv_nid_postfix(&self);
    } catch (const std::system_error& error) {
        selfJoinRejected = error.code().value() != 0;
    }
    Require(selfJoinRejected && self == pthread_self_nid_postfix(), "failed join retains thread handle");

    WorkerState state;
    Pthread thread = nullptr;
    Require(pthread_create_nid_postfix(&thread, nullptr, Worker, &state) == 0, "create joinable thread");
    state.release.store(true);
    _ZNSt3__16thread4joinEv_nid_postfix(&thread);
    Require(thread == nullptr && state.finished.load(), "join waits and clears thread handle");
    _ZNSt3__16threadD1Ev_nid_postfix(&thread);
}

void CheckDetach() {
    WorkerState state;
    Pthread thread = nullptr;
    Require(pthread_create_nid_postfix(&thread, nullptr, Worker, &state) == 0, "create detachable thread");
    _ZNSt3__16thread6detachEv_nid_postfix(&thread);
    Require(thread == nullptr, "detach clears thread handle");
    state.release.store(true);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!state.finished.load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Require(state.finished.load(), "detached thread finishes");
    _ZNSt3__16threadD1Ev_nid_postfix(&thread);
}

}

int main() {
    CheckEmpty();
    CheckJoin();
    CheckDetach();
}
