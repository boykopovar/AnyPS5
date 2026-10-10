#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>

struct GuestOnce {
    std::int32_t state;
    void* mutex;
};
static_assert(sizeof(GuestOnce) == 16 && offsetof(GuestOnce, mutex) == 8);

using Initializer = void (APS5_VABI *)();
extern "C" int APS5_VABI pthread_once_nid_postfix(GuestOnce*, Initializer);

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int einval = 22;
constexpr int workerCount = 16;

struct OnceState {
    GuestOnce nested{};
    std::atomic<int> calls{0};
    std::atomic<int> arrived{0};
    std::atomic<int> nestedResult{-1};
    int published = 0;
    int nestedValue = 0;
    int attempts = 0;
};

OnceState* state = nullptr;

class StateScope {
public:
    StateScope() { state = &value; }
    ~StateScope() { state = nullptr; }
    StateScope(const StateScope&) = delete;
    StateScope& operator=(const StateScope&) = delete;

    OnceState value;
};

void APS5_VABI Nested() {
    state->nestedValue = 17;
}

void APS5_VABI Initialize() {
    ++state->calls;
    while (state->arrived.load() != workerCount) std::this_thread::yield();
    state->nestedResult = pthread_once_nid_postfix(&state->nested, Nested);
    state->published = 42;
}

void APS5_VABI Count() {
    ++state->calls;
}

void APS5_VABI ThrowFirst() {
    if (++state->attempts == 1) throw 7;
}

const Case concurrent{"PthreadOnce_ConcurrentCallers_RunInitializerOnceAndPublishResult", [] {
    StateScope scope;
    GuestOnce control{};
    std::atomic<int> failures{0};
    std::array<std::thread, workerCount> workers;
    for (auto& worker : workers) {
        worker = std::thread([&control, &failures] {
            ++state->arrived;
            if (pthread_once_nid_postfix(&control, Initialize) != 0) ++failures;
            if (state->published != 42 || state->nestedValue != 17) ++failures;
        });
    }
    for (auto& worker : workers) worker.join();
    RequireEqual(failures.load(), 0, "workers that failed or saw unpublished state");
    RequireEqual(scope.value.calls.load(), 1, "initializer calls");
    RequireEqual(scope.value.nestedResult.load(), 0, "nested once result");
    RequireEqual(control.state, 1, "control state");
    Require(control.mutex == nullptr, "control mutex released");
}};

const Case completed{"PthreadOnce_CompletedControl_DoesNotRunInitializerAgain", [] {
    StateScope scope;
    GuestOnce control{};
    RequireEqual(pthread_once_nid_postfix(&control, Count), 0, "first call");
    RequireEqual(pthread_once_nid_postfix(&control, Count), 0, "second call");
    RequireEqual(scope.value.calls.load(), 1, "initializer calls");
}};

const Case throwing{"PthreadOnce_ThrowingInitializer_PropagatesAndLeavesControlRetryable", [] {
    StateScope scope;
    GuestOnce control{};
    try {
        pthread_once_nid_postfix(&control, ThrowFirst);
        Testing::Fail("the initializer exception propagates");
    } catch (int value) {
        RequireEqual(value, 7, "thrown value");
    }
    RequireEqual(control.state, 0, "control state after throw");
    RequireEqual(pthread_once_nid_postfix(&control, ThrowFirst), 0, "retry");
    RequireEqual(scope.value.attempts, 2, "initializer attempts");
}};

const Case nullControl{"PthreadOnce_NullControl_FailsWithEinval", [] {
    StateScope scope;
    RequireEqual(pthread_once_nid_postfix(nullptr, Count), einval, "null control");
}};

const Case nullInitializer{"PthreadOnce_NullInitializer_FailsWithEinval", [] {
    GuestOnce control{1, nullptr};
    RequireEqual(pthread_once_nid_postfix(&control, nullptr), einval, "null initializer");
}};

const Case invalidState{"PthreadOnce_UnknownControlState_FailsWithEinval", [] {
    StateScope scope;
    GuestOnce invalid{9, nullptr};
    RequireEqual(pthread_once_nid_postfix(&invalid, Count), einval, "state 9");
    RequireEqual(scope.value.calls.load(), 0, "initializer calls");
}};

} // namespace
