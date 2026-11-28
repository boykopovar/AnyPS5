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

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JSC microtask check failed at line %u\n", location.line());
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

JSValueRef Eval(JSContextRef ctx, const char* code, std::source_location location = std::source_location::current()) {
    ScopedJsString script(code);
    JSValueRef exception = nullptr;
    JSValueRef value = JSEvaluateScript(ctx, script, nullptr, nullptr, 1, &exception);
    Require(exception == nullptr && value != nullptr, location);
    return value;
}

bool EvalBool(JSContextRef ctx, const char* code, std::source_location location = std::source_location::current()) {
    return JSValueToBoolean(ctx, Eval(ctx, code, location));
}

void SetGlobal(JSContextRef ctx, const char* name, JSValueRef value) {
    ScopedJsString key(name);
    JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), key, value, kJSPropertyAttributeNone, nullptr);
}

JSObjectRef GetObject(JSContextRef ctx, const char* expression) {
    JSObjectRef object = JSValueToObject(ctx, Eval(ctx, expression), nullptr);
    Require(object != nullptr);
    JSValueProtect(ctx, object);
    return object;
}

int reactions = 0;
int rejections = 0;

JSValueRef APS5_VABI CountReaction(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)function;
    (void)thisObject;
    (void)argumentCount;
    (void)arguments;
    (void)exception;
    ++reactions;
    return JSValueMakeUndefined(ctx);
}

JSValueRef APS5_VABI CountRejection(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)function;
    (void)thisObject;
    (void)exception;
    if (argumentCount == 2 && JSValueIsObject(ctx, arguments[0])) ++rejections;
    Eval(ctx, "Promise.resolve().then(countReaction)");
    return JSValueMakeUndefined(ctx);
}

JSValueRef APS5_VABI UseTheApi(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)function;
    (void)thisObject;
    (void)exception;
    if (argumentCount < 1) return JSValueMakeUndefined(ctx);
    JSObjectRef callee = JSValueToObject(ctx, arguments[0], nullptr);
    JSObjectCallAsFunction(ctx, callee, nullptr, 0, nullptr, nullptr);
    Eval(ctx, "log.push('evaluated')");
    ScopedJsString key("touched");
    JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), key, JSValueMakeNumber(ctx, 1.0), kJSPropertyAttributeNone, nullptr);
    JSObjectGetProperty(ctx, JSContextGetGlobalObject(ctx), key, nullptr);
    JSValueToNumber(ctx, arguments[0], nullptr);
    return JSValueMakeUndefined(ctx);
}

void TestNothingRunsBeforeTheScriptEnds(JSContextRef ctx) {
    Require(EvalBool(ctx, "var log = []; Promise.resolve().then(function () { log.push('micro'); });"
                          "useTheApi(function () { log.push('inner'); }); log.push('after'); log.join() === 'inner,evaluated,after'"));
    Require(EvalBool(ctx, "log.join() === 'inner,evaluated,after,micro'"));
}

void TestEachJobFinishesBeforeTheNext(JSContextRef ctx) {
    Require(EvalBool(ctx, "log = [];"
                          "Promise.resolve().then(function () { log.push('a1'); useTheApi(function () { log.push('a-inner'); }); log.push('a2'); });"
                          "Promise.resolve().then(function () { log.push('b'); Promise.resolve().then(function () { log.push('c'); }); }); true"));
    Require(EvalBool(ctx, "log.join() === 'a1,a-inner,evaluated,a2,b,c'"));
}

void TestEveryOutermostCallDrains(JSContextRef ctx) {
    JSObjectRef holder = GetObject(ctx, "({ set trigger(v) { Promise.resolve().then(countReaction); },"
                                        " get reader() { Promise.resolve().then(countReaction); return 1; },"
                                        " valueOf: function () { Promise.resolve().then(countReaction); return 2; },"
                                        " toString: function () { Promise.resolve().then(countReaction); return 'text'; } })");
    JSObjectRef queue = GetObject(ctx, "(function () { Promise.resolve().then(countReaction).then(countReaction).then(countReaction); })");
    JSObjectRef proxy = GetObject(ctx, "new Proxy({}, { has: function () { Promise.resolve().then(countReaction); return true; },"
                                       " deleteProperty: function () { Promise.resolve().then(countReaction); return true; } })");
    ScopedJsString trigger("trigger");
    ScopedJsString reader("reader");

    reactions = 0;
    JSObjectSetProperty(ctx, holder, trigger, JSValueMakeNumber(ctx, 1.0), kJSPropertyAttributeNone, nullptr);
    Require(reactions == 1);
    JSObjectGetProperty(ctx, holder, reader, nullptr);
    Require(reactions == 2);
    Require(JSValueToNumber(ctx, holder, nullptr) == 2.0);
    Require(reactions == 3);
    JSStringRef text = JSValueToStringCopy(ctx, holder, nullptr);
    Require(text != nullptr && reactions == 4);
    JSStringRelease_nid_postfix(text);
    Require(JSObjectHasProperty(ctx, proxy, trigger) && reactions == 5);
    Require(JSObjectDeleteProperty(ctx, proxy, trigger, nullptr) && reactions == 6);
    JSObjectCallAsFunction(ctx, queue, nullptr, 0, nullptr, nullptr);
    Require(reactions == 9);
    JSObjectCallAsConstructor(ctx, queue, 0, nullptr, nullptr);
    Require(reactions == 12);
    Eval(ctx, "Promise.resolve().then(countReaction)");
    Require(reactions == 13);

    JSValueRef exception = nullptr;
    ScopedJsString throwing("Promise.resolve().then(countReaction); throw new Error('thrown');");
    Require(JSEvaluateScript(ctx, throwing, nullptr, nullptr, 1, &exception) == nullptr && exception != nullptr);
    Require(reactions == 14);
    Require(EvalBool(ctx, "Promise.resolve().then(function () { throw 1; }); Promise.resolve().then(countReaction); true"));
    Require(reactions == 15);

    JSValueUnprotect(ctx, proxy);
    JSValueUnprotect(ctx, queue);
    JSValueUnprotect(ctx, holder);
}

void TestUnhandledRejections(JSContextRef ctx) {
    JSGlobalContextRef other = JSGlobalContextCreateInGroup(JSContextGetGroup(ctx), nullptr);
    SetGlobal(other, "countReaction", JSObjectMakeFunctionWithCallback(other, nullptr, &CountReaction));
    JSGlobalContextSetUnhandledRejectionCallback(other, JSObjectMakeFunctionWithCallback(other, nullptr, &CountRejection), nullptr);
    SetGlobal(ctx, "rejectElsewhere", GetObject(other, "(function () { Promise.reject(new Error('unhandled')); })"));

    reactions = 0;
    rejections = 0;
    Require(EvalBool(ctx, "rejectElsewhere(); true"));
    Require(rejections == 1);
    Require(reactions == 1);
    Require(EvalBool(other, "Promise.reject(1); Promise.reject(2).catch(function () {}); true"));
    Require(rejections == 2);
    JSGlobalContextRelease(other);
}

}  // namespace

int main() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    SetGlobal(ctx, "countReaction", JSObjectMakeFunctionWithCallback(ctx, nullptr, &CountReaction));
    SetGlobal(ctx, "useTheApi", JSObjectMakeFunctionWithCallback(ctx, nullptr, &UseTheApi));
    TestNothingRunsBeforeTheScriptEnds(ctx);
    TestEachJobFinishesBeforeTheNext(ctx);
    TestEveryOutermostCallDrains(ctx);
    TestUnhandledRejections(ctx);
    JSGlobalContextRelease(ctx);
    std::puts("libSceNKWebKit JS microtask tests passed");
    return 0;
}
