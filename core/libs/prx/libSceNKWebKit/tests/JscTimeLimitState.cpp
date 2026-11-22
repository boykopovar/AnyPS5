#include "../src/Jsc.hpp"
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

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JscTimeLimitState check failed at line %u\n", location.line());
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

struct ProbeState {
    OpaqueJSContextGroup* group = nullptr;
    int calls = 0;
    int depth = -1;
    bool armed = false;
    Clock::time_point deadline{};
    Clock::duration remaining{};
    bool nested = false;
    double nestedValue = 0.0;
    int nestedDepthAfter = -1;
    bool nestedArmedAfter = false;
    bool nestedKeptDeadline = false;
    double newLimit = 0.0;
    bool armedAfterSet = false;
    bool deadlineFollowsSet = false;
    Clock::duration remainingAfterSet{};
};

ProbeState* g_probe = nullptr;

JSValueRef APS5_VABI Probe(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, std::size_t argumentCount,
    const JSValueRef arguments[], JSValueRef* exception) {
    (void)function;
    (void)thisObject;
    (void)argumentCount;
    (void)arguments;
    (void)exception;
    ProbeState& probe = *g_probe;
    ++probe.calls;
    probe.depth = probe.group->vmEntryDepth;
    probe.armed = probe.group->deadlineArmed;
    probe.deadline = probe.group->deadline;
    probe.remaining = probe.group->deadline - Clock::now();
    if (probe.nested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        JSValueRef inner = Eval(ctx, "1 + 1");
        probe.nestedValue = inner ? JSValueToNumber(ctx, inner, nullptr) : -1.0;
        probe.nestedDepthAfter = probe.group->vmEntryDepth;
        probe.nestedArmedAfter = probe.group->deadlineArmed;
        probe.nestedKeptDeadline = probe.group->deadline == probe.deadline;
    }
    if (probe.newLimit > 0.0) {
        const auto beforeSet = Clock::now();
        JSContextGroupSetExecutionTimeLimit(probe.group, probe.newLimit, nullptr, nullptr);
        probe.armedAfterSet = probe.group->deadlineArmed;
        probe.deadlineFollowsSet = probe.group->deadline - beforeSet >= std::chrono::duration<double>(probe.newLimit);
        probe.remainingAfterSet = probe.group->deadline - Clock::now();
    }
    return JSValueMakeUndefined(ctx);
}

void RequireArmedEntry(const Fixture& f, const ProbeState& probe, Clock::duration limit, Clock::time_point before,
    std::source_location location = std::source_location::current()) {
    Require(probe.calls == 1, location);
    Require(probe.depth == 1, location);
    Require(probe.armed, location);
    Require(probe.remaining <= limit, location);
    Require(probe.deadline >= before + limit, location);
    Require(f.State()->vmEntryDepth == 0, location);
    Require(!f.State()->deadlineArmed, location);
}

void TestEntriesArmAndExitsDisarm() {
    Fixture f;
    ProbeState probe;
    probe.group = f.State();
    g_probe = &probe;
    Clock::time_point before{};
    ScopedJsString name("probe");
    JSObjectRef probeFunction = JSObjectMakeFunctionWithCallback(f.ctx, name, &Probe);
    Require(probeFunction != nullptr);
    JSObjectSetProperty(f.ctx, JSContextGetGlobalObject(f.ctx), name, probeFunction, kJSPropertyAttributeNone, nullptr);
    JSValueRef constructorValue = Eval(f.ctx, "(function () { probe(); })");
    Require(constructorValue != nullptr);
    JSObjectRef constructor = JSValueToObject(f.ctx, constructorValue, nullptr);
    Require(constructor != nullptr);
    JSValueProtect(f.ctx, constructor);
    ScopedJsString source("probe()");
    JSScriptRef script = JSScriptCreateFromString(f.group, nullptr, 1, source, nullptr, nullptr);
    Require(script != nullptr);

    probe.calls = 0;
    before = Clock::now();
    Require(Eval(f.ctx, "probe()") != nullptr);
    Require(probe.calls == 1 && probe.depth == 1 && !probe.armed);
    Require(f.State()->vmEntryDepth == 0 && !f.State()->deadlineArmed);

    const auto limit = std::chrono::seconds(60);
    JSContextGroupSetExecutionTimeLimit(f.group, 60.0, nullptr, nullptr);
    Require(f.State()->hasTimeLimit);
    Require(!f.State()->deadlineArmed);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    probe.calls = 0;
    before = Clock::now();
    Require(Eval(f.ctx, "probe()") != nullptr);
    RequireArmedEntry(f, probe, limit, before);

    probe.calls = 0;
    before = Clock::now();
    Require(JSScriptEvaluate(f.ctx, script, nullptr, nullptr) != nullptr);
    RequireArmedEntry(f, probe, limit, before);

    probe.calls = 0;
    before = Clock::now();
    Require(JSObjectCallAsFunction(f.ctx, probeFunction, nullptr, 0, nullptr, nullptr) != nullptr);
    RequireArmedEntry(f, probe, limit, before);

    probe.calls = 0;
    before = Clock::now();
    Require(JSObjectCallAsConstructor(f.ctx, constructor, 0, nullptr, nullptr) != nullptr);
    RequireArmedEntry(f, probe, limit, before);

    probe.calls = 0;
    before = Clock::now();
    probe.nested = true;
    Require(Eval(f.ctx, "probe()") != nullptr);
    RequireArmedEntry(f, probe, limit, before);
    Require(probe.nestedValue == 2.0);
    Require(probe.nestedDepthAfter == 1);
    Require(probe.nestedArmedAfter);
    Require(probe.nestedKeptDeadline);
    probe.nested = false;

    probe.calls = 0;
    before = Clock::now();
    probe.newLimit = 5.0;
    Require(Eval(f.ctx, "probe()") != nullptr);
    RequireArmedEntry(f, probe, limit, before);
    Require(probe.armedAfterSet);
    Require(probe.remainingAfterSet <= std::chrono::seconds(5));
    Require(probe.deadlineFollowsSet);
    Require(f.State()->timeLimitSeconds == 5.0);
    probe.newLimit = 0.0;

    JSContextGroupClearExecutionTimeLimit(f.group);
    probe.calls = 0;
    before = Clock::now();
    Require(Eval(f.ctx, "probe()") != nullptr);
    Require(probe.calls == 1 && probe.depth == 1 && !probe.armed);

    JSScriptRelease(script);
    g_probe = nullptr;
}

}  // namespace

int main() {
    TestEntriesArmAndExitsDisarm();
    std::puts("libSceNKWebKit JS execution time limit state tests passed");
    return 0;
}
