#include "../src/Jsc.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceJsc/include/Api.hpp"
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <thread>

using namespace NKWebKit;

namespace {

using Clock = std::chrono::steady_clock;

constexpr const char* Spin = "var turns = 0; while (turns < 100000000) { turns++; } turns";
constexpr const char* BoundedLoop = "var n = 0; while (n < 3000000) { n++; } n";

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JscTimeLimit check failed at line %u\n", location.line());
    std::abort();
}

struct ScopedJsString {
    JSStringRef ref = nullptr;
    explicit ScopedJsString(const char* utf8) : ref(JSStringCreateWithUTF8CString_nid_postfix(utf8)) {}
    ScopedJsString(const ScopedJsString&) = delete;
    ~ScopedJsString() {
        if (ref) JSStringRelease_nid_postfix(ref);
    }
    operator JSStringRef() const { return ref; }
};

JSValueRef Eval(JSContextRef ctx, const char* code, JSValueRef* exception = nullptr) {
    ScopedJsString script(code);
    return JSEvaluateScript(ctx, script, nullptr, nullptr, 1, exception);
}

struct Fixture {
    JSContextGroupRef group;
    JSGlobalContextRef ctx;

    Fixture() : group(JSContextGroupCreate()), ctx(JSGlobalContextCreateInGroup(group, nullptr)) {
        Require(group != nullptr && ctx != nullptr);
    }
    Fixture(const Fixture&) = delete;
    ~Fixture() {
        JSGlobalContextRelease(ctx);
        JSContextGroupRelease(group);
    }
    OpaqueJSContextGroup* State() const { return const_cast<OpaqueJSContextGroup*>(group); }
};

void TestLimitStaysSetAfterTermination() {
    Fixture f;
    JSContextGroupSetExecutionTimeLimit(f.group, 0.005, nullptr, nullptr);
    for (int run = 0; run < 3; ++run) {
        Require(Eval(f.ctx, Spin) == nullptr);
        Require(f.State()->hasTimeLimit);
    }
    JSContextGroupClearExecutionTimeLimit(f.group);
    Require(!f.State()->hasTimeLimit);
    JSValueRef exception = nullptr;
    JSValueRef value = Eval(f.ctx, BoundedLoop, &exception);
    Require(value != nullptr && exception == nullptr);
    Require(JSValueToNumber(f.ctx, value, nullptr) == 3000000.0);
}

void TestCallsCountTheLimitFromTheirEntry() {
    Fixture f;
    JSValueRef spin = Eval(f.ctx,
        "(function () { globalThis.turns = 0; while (turns < 100000000) { turns++; } return turns; })");
    Require(spin != nullptr);
    JSObjectRef function = JSValueToObject(f.ctx, spin, nullptr);
    Require(function != nullptr);
    JSValueProtect(f.ctx, function);
    ScopedJsString source(Spin);
    JSScriptRef script = JSScriptCreateFromString(f.group, nullptr, 1, source, nullptr, nullptr);
    Require(script != nullptr);

    const auto limit = std::chrono::milliseconds(60);
    const auto pause = std::chrono::milliseconds(120);
    JSContextGroupSetExecutionTimeLimit(f.group, 0.06, nullptr, nullptr);
    for (int run = 0; run < 2; ++run) {
        std::this_thread::sleep_for(pause);
        auto start = Clock::now();
        Require(JSObjectCallAsFunction(f.ctx, function, nullptr, 0, nullptr, nullptr) == nullptr);
        Require(Clock::now() - start >= limit);

        std::this_thread::sleep_for(pause);
        start = Clock::now();
        Require(JSObjectCallAsConstructor(f.ctx, function, 0, nullptr, nullptr) == nullptr);
        Require(Clock::now() - start >= limit);

        std::this_thread::sleep_for(pause);
        start = Clock::now();
        Require(JSScriptEvaluate(f.ctx, script, nullptr, nullptr) == nullptr);
        Require(Clock::now() - start >= limit);

        std::this_thread::sleep_for(pause);
        start = Clock::now();
        Require(Eval(f.ctx, Spin) == nullptr);
        Require(Clock::now() - start >= limit);
    }
    JSContextGroupClearExecutionTimeLimit(f.group);
    JSScriptRelease(script);
}

struct CountingState {
    int calls = 0;
    int falseAnswers = 0;
    JSContextRef lastContext = nullptr;
};

bool APS5_VABI CountingCallback(JSContextRef ctx, void* context) {
    auto* state = static_cast<CountingState*>(context);
    state->lastContext = ctx;
    ++state->calls;
    return state->calls > state->falseAnswers;
}

void TestCallbackDecides() {
    Fixture f;
    CountingState state;
    state.falseAnswers = 3;
    JSContextGroupSetExecutionTimeLimit(f.group, 0.004, &CountingCallback, &state);
    for (int run = 0; run < 2; ++run) {
        state.calls = 0;
        state.lastContext = nullptr;
        const auto start = Clock::now();
        Require(Eval(f.ctx, Spin) == nullptr);
        Require(Clock::now() - start >= std::chrono::milliseconds(16));
        Require(state.calls == 4);
        Require(state.lastContext == f.ctx);
    }
    state.calls = 0;
    state.falseAnswers = 0;
    Require(Eval(f.ctx, Spin) == nullptr);
    Require(state.calls == 1);
    Require(f.State()->hasTimeLimit);
}

struct ChangingState {
    JSContextGroupRef group = nullptr;
    int firstCalls = 0;
    int secondCalls = 0;
};

bool APS5_VABI SecondCallback(JSContextRef ctx, void* context) {
    (void)ctx;
    ++static_cast<ChangingState*>(context)->secondCalls;
    return true;
}

bool APS5_VABI ExtendingCallback(JSContextRef ctx, void* context) {
    (void)ctx;
    auto* state = static_cast<ChangingState*>(context);
    ++state->firstCalls;
    JSContextGroupSetExecutionTimeLimit(state->group, 0.004, &SecondCallback, state);
    return false;
}

bool APS5_VABI CancellingCallback(JSContextRef ctx, void* context) {
    (void)ctx;
    auto* state = static_cast<ChangingState*>(context);
    ++state->firstCalls;
    JSContextGroupClearExecutionTimeLimit(state->group);
    return false;
}

void TestCallbackChangesTheLimit() {
    Fixture f;
    ChangingState extending;
    extending.group = f.group;
    JSContextGroupSetExecutionTimeLimit(f.group, 0.004, &ExtendingCallback, &extending);
    Require(Eval(f.ctx, Spin) == nullptr);
    Require(extending.firstCalls == 1);
    Require(extending.secondCalls == 1);

    ChangingState cancelling;
    cancelling.group = f.group;
    JSContextGroupSetExecutionTimeLimit(f.group, 0.004, &CancellingCallback, &cancelling);
    for (int run = 0; run < 2; ++run) {
        JSValueRef exception = nullptr;
        JSValueRef value = Eval(f.ctx, BoundedLoop, &exception);
        Require(value != nullptr && exception == nullptr);
        Require(JSValueToNumber(f.ctx, value, nullptr) == 3000000.0);
        Require(cancelling.firstCalls == 1);
        Require(!f.State()->hasTimeLimit);
    }
}

void TestPassedDeadlineDoesNotStopLaterWork() {
    Fixture f;
    JSValueRef object = Eval(f.ctx, "({ valueOf() { var n = 0; for (var i = 0; i < 300000; i++) n++; return n; } })");
    Require(object != nullptr);
    JSValueProtect(f.ctx, object);
    JSContextGroupSetExecutionTimeLimit(f.group, 0.03, nullptr, nullptr);
    Require(JSValueToNumber(f.ctx, Eval(f.ctx, "7"), nullptr) == 7.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    JSValueRef exception = nullptr;
    Require(JSValueToNumber(f.ctx, object, &exception) == 300000.0);
    Require(exception == nullptr);
    JSContextGroupClearExecutionTimeLimit(f.group);
}

struct NapState {
    int naps = 0;
    double processorAtWake = 0;
};

NapState napState;

JSValueRef APS5_VABI Nap(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)function;
    (void)thisObject;
    (void)argumentCount;
    (void)arguments;
    (void)exception;
    ++napState.naps;
    const auto wake = Clock::now() + std::chrono::milliseconds(400);
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    while (Clock::now() < wake) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    napState.processorAtWake = _ZN3WTF7CPUTime16forCurrentThreadEv().m_value;
    return JSValueMakeUndefined(ctx);
}

double ProcessorSeconds() {
    return _ZN3WTF7CPUTime16forCurrentThreadEv().m_value;
}

void TestTimeSpentWaitingIsNotCounted() {
    Fixture f;
    ScopedJsString name("nap");
    JSObjectSetProperty(f.ctx, JSContextGetGlobalObject(f.ctx), name, JSObjectMakeFunctionWithCallback(f.ctx, name, &Nap), kJSPropertyAttributeNone, nullptr);
    constexpr const char* NapThenWork = "nap(); var n = 0; while (n < 300000) { n++; } n";
    const auto nap = std::chrono::milliseconds(400);

    CountingState state;
    JSContextGroupSetExecutionTimeLimit(f.group, 0.25, &CountingCallback, &state);
    JSValueRef exception = nullptr;
    auto start = Clock::now();
    JSValueRef value = Eval(f.ctx, NapThenWork, &exception);
    Require(Clock::now() - start >= nap);
    Require(value != nullptr && exception == nullptr);
    Require(JSValueToNumber(f.ctx, value, nullptr) == 300000.0);
    Require(state.calls == 0);

    JSContextGroupSetExecutionTimeLimit(f.group, 0.25, nullptr, nullptr);
    value = Eval(f.ctx, NapThenWork, &exception);
    Require(value != nullptr && exception == nullptr);
    Require(JSValueToNumber(f.ctx, value, nullptr) == 300000.0);
    Require(napState.naps == 2);

    JSContextGroupSetExecutionTimeLimit(f.group, 0.05, nullptr, nullptr);
    const double processorBefore = ProcessorSeconds();
    start = Clock::now();
    Require(Eval(f.ctx, "nap(); var turns = 0; while (turns < 100000000) { turns++; } turns") == nullptr);
    Require(ProcessorSeconds() - processorBefore >= 0.05);
    const std::chrono::duration<double> limitLeftAtWake(0.05 - (napState.processorAtWake - processorBefore));
    Require(Clock::now() - start >= nap + limitLeftAtWake);
    JSContextGroupClearExecutionTimeLimit(f.group);
}

}  // namespace

int main() {
    TestLimitStaysSetAfterTermination();
    TestCallsCountTheLimitFromTheirEntry();
    TestCallbackDecides();
    TestCallbackChangesTheLimit();
    TestPassedDeadlineDoesNotStopLaterWork();
    TestTimeSpentWaitingIsNotCounted();
    std::puts("libSceNKWebKit JS execution time limit tests passed");
    return 0;
}
