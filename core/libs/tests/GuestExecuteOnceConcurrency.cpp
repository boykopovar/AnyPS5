#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <thread>

using Callback = int (APS5_VABI *)(void*, void*, void**);
using ExecuteFunction = int (APS5_VABI *)(int*, Callback, void*);

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Execute-once concurrency check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static ExecuteFunction execute = nullptr;

struct IndependentState {
    int outer = 0;
    int inner = 0;
    int published = 0;
    int calls = 0;
};

static int APS5_VABI InitializeInner(void*, void* argument, void**) {
    auto& state = *static_cast<IndependentState*>(argument);
    ++state.calls;
    state.published = 42;
    return 1;
}

static int APS5_VABI InitializeOuter(void*, void* argument, void**) {
    auto& state = *static_cast<IndependentState*>(argument);
    std::thread worker([&] {
        Require(execute(&state.inner, InitializeInner, &state) == 1);
    });
    worker.join();
    Require(state.published == 42);
    return 1;
}

static void IndependentFlags() {
    IndependentState state;
    std::promise<void> completed;
    auto ready = completed.get_future();
    std::thread worker([&] {
        Require(execute(&state.outer, InitializeOuter, &state) == 1);
        completed.set_value();
    });
    Require(ready.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    worker.join();
    Require(state.outer == 1 && state.inner == 1 && state.calls == 1 && state.published == 42);
}

struct RetryState {
    int flag = 0;
    int published = 0;
    bool throwing = false;
    std::atomic<unsigned> arrived = 0;
    std::atomic<unsigned> attempts = 0;
    std::atomic<unsigned> failures = 0;
    std::atomic<unsigned> exceptions = 0;
};

static constexpr unsigned Workers = 16;

static int APS5_VABI Retry(void*, void* argument, void**) {
    auto& state = *static_cast<RetryState*>(argument);
    const auto attempt = ++state.attempts;
    Require(attempt <= 2);
    if (attempt == 1) {
        while (state.arrived.load() != Workers) std::this_thread::yield();
        if (state.throwing) throw 17;
        return 0;
    }
    state.published = 73;
    return 5;
}

static void ConcurrentRetry(bool throwing) {
    RetryState state;
    state.throwing = throwing;
    std::array<std::thread, Workers> workers;
    for (auto& worker : workers) {
        worker = std::thread([&] {
            ++state.arrived;
            try {
                const auto result = execute(&state.flag, Retry, &state);
                if (result == 0) ++state.failures;
                else Require(result == 1 && state.published == 73);
            } catch (int value) {
                Require(value == 17);
                ++state.exceptions;
            }
        });
    }
    for (auto& worker : workers) worker.join();
    Require(state.flag == 1 && state.published == 73 && state.attempts == 2);
    Require(state.failures == (throwing ? 0u : 1u));
    Require(state.exceptions == (throwing ? 1u : 0u));
}

int main(int argc, char** argv) {
    Require(argc == 2);
    if (std::strcmp(argv[1], "mangled") == 0) execute = _ZSt13_Execute_onceRSt9once_flagPFiPvS1_PS1_ES1__nid_postfix;
    else {
        Require(std::strcmp(argv[1], "plain") == 0);
        execute = std_execute_once_nid_postfix;
    }
    IndependentFlags();
    ConcurrentRetry(false);
    ConcurrentRetry(true);
    IndependentFlags();
}
