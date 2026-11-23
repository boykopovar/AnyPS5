#include "../src/Jsc.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceJsc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>

using namespace NKWebKit;

namespace {

constexpr std::size_t MiB = 1024 * 1024;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JSC stack overflow check failed at line %u\n", location.line());
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

JSValueRef Eval(JSContextRef ctx, const char* code, JSValueRef* exception) {
    ScopedJsString script(code);
    return JSEvaluateScript(ctx, script, nullptr, nullptr, 1, exception);
}

bool EvalBool(JSContextRef ctx, const char* code, std::source_location location = std::source_location::current()) {
    JSValueRef exception = nullptr;
    JSValueRef value = Eval(ctx, code, &exception);
    Require(exception == nullptr && value != nullptr, location);
    return JSValueToBoolean(ctx, value);
}

void SetGlobal(JSContextRef ctx, const char* name, JSValueRef value) {
    ScopedJsString key(name);
    JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), key, value, kJSPropertyAttributeNone, nullptr);
}

void RequireRangeError(JSContextRef ctx, JSValueRef exception, std::source_location location = std::source_location::current()) {
    Require(exception != nullptr, location);
    SetGlobal(ctx, "lastError", exception);
    Require(EvalBool(ctx, "lastError instanceof RangeError", location), location);
}

JSObjectRef recursiveFunction = nullptr;

bool APS5_VABI RecursingHasInstance(JSContextRef ctx, JSObjectRef constructor, JSValueRef possibleInstance, JSValueRef* exception) {
    (void)constructor;
    (void)possibleInstance;
    JSValueRef result = JSObjectCallAsFunction(ctx, recursiveFunction, nullptr, 0, nullptr, exception);
    return result != nullptr && JSValueToBoolean(ctx, result);
}

double RunRecursionChecks(JSContextRef ctx) {
    JSValueRef exception = nullptr;
    Require(Eval(ctx, "(function () { function endless() { return endless(); } return endless(); })()", &exception) == nullptr);
    RequireRangeError(ctx, exception);

    exception = nullptr;
    JSValueRef finite = Eval(ctx, "(function () { function count(n) { return n ? count(n - 1) + 1 : 0; } return count(40); })()", &exception);
    Require(finite != nullptr && exception == nullptr);
    Require(JSValueToNumber(ctx, finite, nullptr) == 40.0);

    exception = nullptr;
    JSValueRef endlessValue = Eval(ctx, "(function endless() { return endless(); })", &exception);
    Require(endlessValue != nullptr && exception == nullptr);
    JSObjectRef endless = JSValueToObject(ctx, endlessValue, nullptr);
    Require(endless != nullptr);
    JSValueProtect(ctx, endless);
    Require(JSObjectCallAsFunction(ctx, endless, nullptr, 0, nullptr, &exception) == nullptr);
    RequireRangeError(ctx, exception);
    exception = nullptr;
    Require(JSObjectCallAsConstructor(ctx, endless, 0, nullptr, &exception) == nullptr);
    RequireRangeError(ctx, exception);

    exception = nullptr;
    JSValueRef evil = Eval(ctx, "({ valueOf: function again() { return again(); }, toString: function more() { return more(); } })", &exception);
    Require(evil != nullptr && exception == nullptr);
    Require(std::isnan(JSValueToNumber(ctx, evil, &exception)));
    RequireRangeError(ctx, exception);
    exception = nullptr;
    Require(JSValueToStringCopy(ctx, evil, &exception) == nullptr);
    RequireRangeError(ctx, exception);

    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.hasInstance = &RecursingHasInstance;
    JSClassRef recursingClass = JSClassCreate(&definition);
    Require(recursingClass != nullptr);
    recursiveFunction = endless;
    SetGlobal(ctx, "Recursing", JSObjectMake(ctx, recursingClass, nullptr));
    exception = nullptr;
    Require(Eval(ctx, "undefined instanceof Recursing", &exception) == nullptr);
    RequireRangeError(ctx, exception);
    recursiveFunction = nullptr;
    JSClassRelease(recursingClass);
    JSValueUnprotect(ctx, endless);

    exception = nullptr;
    JSValueRef reached = Eval(ctx, "(function () { var depth = 0; function down() { depth++; down(); } try { down(); } catch (error) {} return depth; })()", &exception);
    Require(reached != nullptr && exception == nullptr);
    Require(EvalBool(ctx, "1 + 1 === 2"));
    return JSValueToNumber(ctx, reached, nullptr);
}

struct ThreadRun {
    JSGlobalContextRef shared = nullptr;
    double depth = -1.0;
};

void* APS5_VABI RecursionThread(void* argument) {
    auto* run = static_cast<ThreadRun*>(argument);
    JSGlobalContextRef ctx = run->shared ? run->shared : JSGlobalContextCreate(nullptr);
    run->depth = RunRecursionChecks(ctx);
    if (!run->shared) JSGlobalContextRelease(ctx);
    return nullptr;
}

double DepthOnAThread(std::size_t stackSize, JSGlobalContextRef shared = nullptr) {
    PthreadAttr attr = nullptr;
    Require(pthread_attr_init_nid_postfix(&attr) == 0);
    Require(pthread_attr_setstacksize_nid_postfix(&attr, stackSize) == 0);
    Pthread thread = nullptr;
    ThreadRun run;
    run.shared = shared;
    Require(pthread_create_nid_postfix(&thread, &attr, &RecursionThread, &run) == 0);
    Require(pthread_join_nid_postfix(thread, nullptr) == 0);
    Require(pthread_attr_destroy_nid_postfix(&attr) == 0);
    Require(run.depth >= 40.0);
    return run.depth;
}

void TestOnThisThread() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    const double first = RunRecursionChecks(ctx);
    const double second = RunRecursionChecks(ctx);
    Require(first >= 40.0 && second >= 40.0);
    JSGlobalContextRelease(ctx);
}

void TestTheLimitFollowsTheStackOfTheThread() {
    const double one = DepthOnAThread(1 * MiB);
    const double four = DepthOnAThread(4 * MiB);
    const double sixteen = DepthOnAThread(16 * MiB);
    const double thirtyTwo = DepthOnAThread(32 * MiB);
    Require(four > 2.0 * one);
    Require(sixteen > four);
    Require(sixteen < 1.6 * four);
    Require(std::fabs(thirtyTwo - sixteen) <= sixteen / 20.0);
}

void TestAContextSharedBetweenThreads() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    Require(RunRecursionChecks(ctx) >= 40.0);
    const double small = DepthOnAThread(1 * MiB, ctx);
    const double large = DepthOnAThread(4 * MiB, ctx);
    Require(large > 2.0 * small);
    Require(RunRecursionChecks(ctx) >= 40.0);
    JSGlobalContextRelease(ctx);
}

void* APS5_VABI NoRoomThread(void* argument) {
    auto* run = static_cast<ThreadRun*>(argument);
    JSValueRef exception = nullptr;
    JSValueRef value = Eval(run->shared, "1", &exception);
    run->depth = value == nullptr && exception != nullptr ? 1.0 : 0.0;
    return nullptr;
}

void TestNoRoomAboveTheReservedZone() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    PthreadAttr attr = nullptr;
    Require(pthread_attr_init_nid_postfix(&attr) == 0);
    Require(pthread_attr_setstacksize_nid_postfix(&attr, 128 * 1024) == 0);
    Pthread thread = nullptr;
    ThreadRun run;
    run.shared = ctx;
    Require(pthread_create_nid_postfix(&thread, &attr, &NoRoomThread, &run) == 0);
    Require(pthread_join_nid_postfix(thread, nullptr) == 0);
    Require(pthread_attr_destroy_nid_postfix(&attr) == 0);
    Require(run.depth == 1.0);
    Require(EvalBool(ctx, "1 + 1 === 2"));
    JSGlobalContextRelease(ctx);
}

}  // namespace

int main() {
    TestOnThisThread();
    TestNoRoomAboveTheReservedZone();
    TestTheLimitFollowsTheStackOfTheThread();
    TestAContextSharedBetweenThreads();
    std::puts("libSceNKWebKit JS stack overflow tests passed");
    return 0;
}
