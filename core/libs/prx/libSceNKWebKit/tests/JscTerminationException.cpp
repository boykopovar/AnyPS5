#include "../src/Jsc.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceJsc/include/Api.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>

using namespace NKWebKit;

namespace {

constexpr const char* Spin = "var turns = 0; while (turns < 100000000) { turns++; } turns";

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JSC termination exception check failed at line %u\n", location.line());
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

JSObjectRef EvalObject(JSContextRef ctx, const char* code) {
    JSObjectRef object = JSValueToObject(ctx, Eval(ctx, code), nullptr);
    Require(object != nullptr);
    JSValueProtect(ctx, object);
    return object;
}

void RequireTheTerminationString(JSContextRef ctx, JSValueRef exception, std::source_location location = std::source_location::current()) {
    Require(exception != nullptr, location);
    Require(JSValueIsString(ctx, exception) && !JSValueIsObject(ctx, exception) && JSValueGetType(ctx, exception) == kJSTypeString, location);
    JSStringRef text = JSValueToStringCopy(ctx, exception, nullptr);
    Require(text != nullptr && JSStringIsEqualToUTF8CString(text, "JavaScript execution terminated."), location);
    JSStringRelease_nid_postfix(text);
}

void TestEveryWayIntoScript() {
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef ctx = JSGlobalContextCreateInGroup(group, nullptr);
    JSObjectRef spinning = EvalObject(ctx, "(function () { var turns = 0; while (turns < 100000000) { turns++; } return turns; })");
    JSObjectRef holder = EvalObject(ctx, "({ get slow() { var turns = 0; while (turns < 100000000) { turns++; } return turns; },"
                                         " valueOf: function () { var turns = 0; while (turns < 100000000) { turns++; } return turns; } })");
    ScopedJsString source(Spin);
    JSScriptRef script = JSScriptCreateFromString(group, nullptr, 1, source, nullptr, nullptr);
    Require(script != nullptr);
    ScopedJsString slow("slow");
    JSContextGroupSetExecutionTimeLimit(group, 0.005, nullptr, nullptr);

    JSValueRef exception = nullptr;
    Require(Eval(ctx, Spin, &exception) == nullptr);
    RequireTheTerminationString(ctx, exception);

    exception = nullptr;
    Require(JSObjectCallAsFunction(ctx, spinning, nullptr, 0, nullptr, &exception) == nullptr);
    RequireTheTerminationString(ctx, exception);

    exception = nullptr;
    Require(JSObjectCallAsConstructor(ctx, spinning, 0, nullptr, &exception) == nullptr);
    RequireTheTerminationString(ctx, exception);

    exception = nullptr;
    Require(JSScriptEvaluate(ctx, script, nullptr, &exception) == nullptr);
    RequireTheTerminationString(ctx, exception);

    exception = nullptr;
    JSObjectGetProperty(ctx, holder, slow, &exception);
    RequireTheTerminationString(ctx, exception);

    exception = nullptr;
    JSValueToNumber(ctx, holder, &exception);
    RequireTheTerminationString(ctx, exception);

    Require(Eval(ctx, Spin, nullptr) == nullptr);

    exception = nullptr;
    Require(Eval(ctx, "var caught = false; try { var turns = 0; while (turns < 100000000) { turns++; } } catch (e) { caught = true; } turns", &exception) == nullptr);
    RequireTheTerminationString(ctx, exception);
    JSContextGroupClearExecutionTimeLimit(group);
    exception = nullptr;
    JSValueRef caught = Eval(ctx, "caught", &exception);
    Require(caught != nullptr && exception == nullptr && !JSValueToBoolean(ctx, caught));

    JSScriptRelease(script);
    JSValueUnprotect(ctx, holder);
    JSValueUnprotect(ctx, spinning);
    JSGlobalContextRelease(ctx);
    JSContextGroupRelease(group);
}

void TestOtherExceptionsAreLeftAlone() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    JSValueRef exception = nullptr;
    Require(Eval(ctx, "throw new RangeError('mine');", &exception) == nullptr);
    Require(exception != nullptr && JSValueIsObject(ctx, exception) && !JSValueIsString(ctx, exception));

    exception = nullptr;
    Require(Eval(ctx, "throw 'JavaScript execution terminated.';", &exception) == nullptr);
    Require(exception != nullptr && JSValueIsString(ctx, exception));

    exception = nullptr;
    JSValueRef value = Eval(ctx, "var seen = null; try { throw 'JavaScript execution terminated.'; } catch (e) { seen = e; } seen", &exception);
    Require(value != nullptr && exception == nullptr && JSValueIsString(ctx, value));
    JSGlobalContextRelease(ctx);
}

}  // namespace

int main() {
    TestEveryWayIntoScript();
    TestOtherExceptionsAreLeftAlone();
    std::puts("libSceNKWebKit JS termination exception tests passed");
    return 0;
}
