#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <string>
#include <thread>

using Callback = int (APS5_VABI *)(void*, void*, void**);
extern "C" int APS5_VABI _ZSt13_Execute_onceRSt9once_flagPFiPvS1_PS1_ES1__nid_postfix(int*, Callback, void*);

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr std::size_t workerCount = 16;

int ExecuteOnce(int* flag, Callback callback, void* arg) {
    return _ZSt13_Execute_onceRSt9once_flagPFiPvS1_PS1_ES1__nid_postfix(flag, callback, arg);
}

struct NestedState {
    int value = 17;
    int seen = 0;
};

struct InitializeState {
    std::atomic<int> calls{0};
    std::atomic<int> arrived{0};
    int nestedFlag = 0;
    int nestedResult = 0;
    NestedState nested;
    int published = 0;
    void* seenFirst = &published;
    void* seenArg = nullptr;
    void** seenThird = reinterpret_cast<void**>(&published);
};

int APS5_VABI Nested(void*, void* arg, void**) {
    auto* state = static_cast<NestedState*>(arg);
    state->seen = state->value;
    return 1;
}

int APS5_VABI Initialize(void* first, void* arg, void** third) {
    auto* state = static_cast<InitializeState*>(arg);
    ++state->calls;
    state->seenFirst = first;
    state->seenArg = arg;
    state->seenThird = third;
    while (state->arrived.load() != static_cast<int>(workerCount)) std::this_thread::yield();
    state->nestedResult = ExecuteOnce(&state->nestedFlag, Nested, &state->nested);
    state->published = 42;
    return 5;
}

int APS5_VABI CountAndFail(void*, void* arg, void**) {
    ++*static_cast<int*>(arg);
    return 0;
}

int APS5_VABI ThrowOnFirstCall(void*, void* arg, void**) {
    if (++*static_cast<int*>(arg) == 1) throw 7;
    return 1;
}

int APS5_VABI CountUnexpectedCall(void*, void* arg, void**) {
    ++*static_cast<int*>(arg);
    return 1;
}

struct ConcurrentRun {
    InitializeState state;
    int control = 0;
    std::array<int, workerCount> results{};
    std::array<bool, workerCount> sawPublished{};
};

void RunConcurrently(ConcurrentRun& run) {
    std::array<std::thread, workerCount> workers;
    for (std::size_t index = 0; index < workers.size(); ++index) {
        workers[index] = std::thread([&run, index] {
            ++run.state.arrived;
            run.results[index] = ExecuteOnce(&run.control, Initialize, &run.state);
            run.sawPublished[index] = run.state.published == 42 && run.state.nested.seen == 17;
        });
    }
    for (auto& worker : workers) worker.join();
}

const Case concurrentOnce{"ExecuteOnce_ConcurrentCallers_RunCallbackOnceAndSetFlag", [] {
    ConcurrentRun run;
    RunConcurrently(run);
    RequireEqual(run.state.calls.load(), 1, "callback calls");
    RequireEqual(run.control, 1, "flag");
}};

const Case concurrentResults{"ExecuteOnce_ConcurrentCallers_AllReturnOne", [] {
    ConcurrentRun run;
    RunConcurrently(run);
    for (std::size_t index = 0; index < workerCount; ++index) {
        RequireEqual(run.results[index], 1, "result of worker " + std::to_string(index));
    }
}};

const Case concurrentPublication{"ExecuteOnce_ConcurrentCallers_SeeCallbackEffectsAfterReturn", [] {
    ConcurrentRun run;
    RunConcurrently(run);
    for (std::size_t index = 0; index < workerCount; ++index) {
        Require(run.sawPublished[index], "worker " + std::to_string(index) + " saw the published values");
    }
}};

const Case nestedCall{"ExecuteOnce_NestedCallInsideCallback_CompletesInnerFlag", [] {
    ConcurrentRun run;
    RunConcurrently(run);
    RequireEqual(run.state.nestedResult, 1, "nested result");
    RequireEqual(run.state.nestedFlag, 1, "nested flag");
    RequireEqual(run.state.nested.seen, 17, "nested argument");
}};

const Case callbackArguments{"ExecuteOnce_Callback_ReceivesNullFirstAndThirdAndCallerArgument", [] {
    ConcurrentRun run;
    RunConcurrently(run);
    Require(run.state.seenFirst == nullptr, "first argument is null");
    Require(run.state.seenArg == &run.state, "second argument is the caller argument");
    Require(run.state.seenThird == nullptr, "third argument is null");
}};

const Case completedFlag{"ExecuteOnce_CompletedFlag_SkipsCallbackAndReturnsOne", [] {
    ConcurrentRun run;
    RunConcurrently(run);
    int unexpected = 0;
    RequireEqual(ExecuteOnce(&run.control, CountUnexpectedCall, &unexpected), 1, "result");
    RequireEqual(unexpected, 0, "callback calls");
    RequireEqual(run.state.calls.load(), 1, "initializer calls");
}};

const Case failingCallback{"ExecuteOnce_FailingCallback_ReturnsZeroAndLeavesFlagClear", [] {
    int flag = 0;
    int failures = 0;
    RequireEqual(ExecuteOnce(&flag, CountAndFail, &failures), 0, "result");
    RequireEqual(flag, 0, "flag");
    RequireEqual(failures, 1, "callback calls");
}};

const Case failingRetry{"ExecuteOnce_FailingCallbackRetried_RunsCallbackAgain", [] {
    int flag = 0;
    int failures = 0;
    ExecuteOnce(&flag, CountAndFail, &failures);
    RequireEqual(ExecuteOnce(&flag, CountAndFail, &failures), 0, "second result");
    RequireEqual(flag, 0, "flag");
    RequireEqual(failures, 2, "callback calls");
}};

const Case throwingCallback{"ExecuteOnce_ThrowingCallback_PropagatesExceptionAndLeavesFlagClear", [] {
    int flag = 0;
    int throws = 0;
    const int thrown = Testing::RequireThrows<int>([&] { ExecuteOnce(&flag, ThrowOnFirstCall, &throws); }, "first call");
    RequireEqual(thrown, 7, "thrown value");
    RequireEqual(flag, 0, "flag");
}};

const Case throwingRetry{"ExecuteOnce_ThrowingCallbackRetried_SucceedsAndSetsFlag", [] {
    int flag = 0;
    int throws = 0;
    Testing::RequireThrows<int>([&] { ExecuteOnce(&flag, ThrowOnFirstCall, &throws); }, "first call");
    RequireEqual(ExecuteOnce(&flag, ThrowOnFirstCall, &throws), 1, "second result");
    RequireEqual(flag, 1, "flag");
    RequireEqual(throws, 2, "callback calls");
}};

const Case presetFlag{"ExecuteOnce_PresetNonZeroFlag_SkipsCallbackAndKeepsFlag", [] {
    int preset = -3;
    int unexpected = 0;
    RequireEqual(ExecuteOnce(&preset, CountUnexpectedCall, &unexpected), 1, "result");
    RequireEqual(preset, -3, "flag");
    RequireEqual(unexpected, 0, "callback calls");
}};

} // namespace
