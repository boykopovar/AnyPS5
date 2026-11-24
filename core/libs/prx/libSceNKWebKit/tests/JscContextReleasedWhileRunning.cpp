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
    std::fprintf(stderr, "JSC context released while running check failed at line %u\n", location.line());
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

enum Action {
    kRelease = 1,
    kReleaseThenRetain = 2,
    kReleaseRetainRelease = 3,
    kReleaseTwice = 4,
    kReleaseTheOther = 5,
    kReleaseInAnInnerScript = 6,
};

int finalized = 0;
int finalizedInsideTheCallback = -1;
int finalizedAfterTheInnerScript = -1;
int reactions = 0;
JSGlobalContextRef theOther = nullptr;

void APS5_VABI CountFinalize(JSObjectRef object) {
    (void)object;
    ++finalized;
}

JSValueRef Eval(JSContextRef ctx, const char* code, std::source_location location = std::source_location::current()) {
    ScopedJsString script(code);
    JSValueRef exception = nullptr;
    JSValueRef value = JSEvaluateScript(ctx, script, nullptr, nullptr, 1, &exception);
    Require(exception == nullptr && value != nullptr, location);
    return value;
}

JSValueRef APS5_VABI CountReaction(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)function;
    (void)thisObject;
    (void)argumentCount;
    (void)arguments;
    (void)exception;
    ++reactions;
    return JSValueMakeUndefined(ctx);
}

JSValueRef APS5_VABI Act(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)function;
    (void)thisObject;
    (void)exception;
    JSGlobalContextRef self = JSContextGetGlobalContext(ctx);
    const int action = argumentCount ? static_cast<int>(JSValueToNumber(ctx, arguments[0], nullptr)) : 0;
    if (action == kRelease) {
        JSGlobalContextRelease(self);
    } else if (action == kReleaseThenRetain) {
        JSGlobalContextRelease(self);
        JSGlobalContextRetain(self);
    } else if (action == kReleaseRetainRelease) {
        JSGlobalContextRelease(self);
        JSGlobalContextRetain(self);
        JSGlobalContextRelease(self);
    } else if (action == kReleaseTwice) {
        JSGlobalContextRelease(self);
        JSGlobalContextRelease(self);
    } else if (action == kReleaseTheOther) {
        JSGlobalContextRelease(theOther);
        theOther = nullptr;
    } else if (action == kReleaseInAnInnerScript) {
        Eval(ctx, "act(1); marker = 'inner'");
        finalizedAfterTheInnerScript = finalized;
    }
    finalizedInsideTheCallback = finalized;
    ScopedJsString key("touched");
    JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), key, JSValueMakeNumber(ctx, 5.0), kJSPropertyAttributeNone, nullptr);
    return JSValueMakeNumber(ctx, 7.0);
}

JSClassRef GlobalClass() {
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.finalize = &CountFinalize;
    return JSClassCreate(&definition);
}

JSGlobalContextRef MakeContext(JSContextGroupRef group) {
    JSClassRef globalClass = GlobalClass();
    JSGlobalContextRef ctx = JSGlobalContextCreateInGroup(group, globalClass);
    JSClassRelease(globalClass);
    ScopedJsString act("act");
    JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), act, JSObjectMakeFunctionWithCallback(ctx, nullptr, &Act), kJSPropertyAttributeNone, nullptr);
    ScopedJsString countReaction("countReaction");
    JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), countReaction, JSObjectMakeFunctionWithCallback(ctx, nullptr, &CountReaction), kJSPropertyAttributeNone, nullptr);
    return ctx;
}

void Reset() {
    finalized = 0;
    finalizedInsideTheCallback = -1;
    finalizedAfterTheInnerScript = -1;
    reactions = 0;
}

void TestTheScriptGoesOn() {
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef keeper = JSGlobalContextCreateInGroup(group, nullptr);
    JSGlobalContextRef ctx = MakeContext(group);
    Reset();
    ScopedJsString script("var r = act(1); var doubled = [1, 2, 3].map(function (x) { return x * 2; }).join(); r + touched + doubled.length");
    JSValueRef exception = nullptr;
    JSValueRef result = JSEvaluateScript(ctx, script, nullptr, nullptr, 1, &exception);
    Require(exception == nullptr && result != nullptr);
    Require(finalizedInsideTheCallback == 0);
    Require(finalized == 1);
    Require(JSValueToNumber(keeper, result, nullptr) == 17.0);
    JSGlobalContextRelease(keeper);
    JSContextGroupRelease(group);
}

void TestTheLastContextOfItsGroup() {
    JSGlobalContextRef ctx = MakeContext(nullptr);
    Reset();
    ScopedJsString script("Promise.resolve().then(countReaction); var r = act(1); r + touched");
    JSEvaluateScript(ctx, script, nullptr, nullptr, 1, nullptr);
    Require(finalizedInsideTheCallback == 0);
    Require(finalized == 1);
    Require(reactions == 1);
}

void TestThroughACall() {
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef keeper = JSGlobalContextCreateInGroup(group, nullptr);
    JSGlobalContextRef ctx = MakeContext(group);
    Reset();
    JSObjectRef act = JSObjectMakeFunctionWithCallback(ctx, nullptr, &Act);
    JSValueProtect(keeper, act);
    JSValueRef argument = JSValueMakeNumber(ctx, kRelease);
    JSValueRef result = JSObjectCallAsFunction(ctx, act, nullptr, 1, &argument, nullptr);
    Require(finalizedInsideTheCallback == 0);
    Require(finalized == 1);
    Require(result != nullptr && JSValueToNumber(keeper, result, nullptr) == 7.0);
    JSValueUnprotect(keeper, act);
    JSGlobalContextRelease(keeper);
    JSContextGroupRelease(group);

    JSGlobalContextRef alone = MakeContext(nullptr);
    Reset();
    JSObjectRef actAlone = JSObjectMakeFunctionWithCallback(alone, nullptr, &Act);
    JSValueRef argumentAlone = JSValueMakeNumber(alone, kRelease);
    JSObjectCallAsFunction(alone, actAlone, nullptr, 1, &argumentAlone, nullptr);
    Require(finalizedInsideTheCallback == 0);
    Require(finalized == 1);
}

void TestTheOutermostCallDecides() {
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef keeper = JSGlobalContextCreateInGroup(group, nullptr);
    JSGlobalContextRef ctx = MakeContext(group);
    Reset();
    ScopedJsString script("var marker = 'outer'; act(6); marker + touched");
    JSValueRef result = JSEvaluateScript(ctx, script, nullptr, nullptr, 1, nullptr);
    Require(finalizedAfterTheInnerScript == 0);
    Require(finalizedInsideTheCallback == 0);
    Require(finalized == 1);
    JSStringRef text = JSValueToStringCopy(keeper, result, nullptr);
    Require(text != nullptr && JSStringIsEqualToUTF8CString(text, "inner5"));
    JSStringRelease_nid_postfix(text);
    JSGlobalContextRelease(keeper);
    JSContextGroupRelease(group);
}

void TestRetainedAgain() {
    JSGlobalContextRef ctx = MakeContext(nullptr);
    Reset();
    Require(JSValueToNumber(ctx, Eval(ctx, "act(2) + touched"), nullptr) == 12.0);
    Require(finalized == 0);
    Require(JSValueToNumber(ctx, Eval(ctx, "touched + 1"), nullptr) == 6.0);
    JSGlobalContextRelease(ctx);
    Require(finalized == 1);

    JSGlobalContextRef again = MakeContext(nullptr);
    Reset();
    ScopedJsString script("act(3)");
    JSEvaluateScript(again, script, nullptr, nullptr, 1, nullptr);
    Require(finalizedInsideTheCallback == 0);
    Require(finalized == 1);
}

void TestRetainCounts() {
    JSGlobalContextRef ctx = MakeContext(nullptr);
    JSGlobalContextRetain(ctx);
    Reset();
    ScopedJsString script("act(4)");
    JSEvaluateScript(ctx, script, nullptr, nullptr, 1, nullptr);
    Require(finalizedInsideTheCallback == 0);
    Require(finalized == 1);

    JSGlobalContextRef kept = MakeContext(nullptr);
    JSGlobalContextRetain(kept);
    Reset();
    Require(JSValueToNumber(kept, Eval(kept, "act(1) + touched"), nullptr) == 12.0);
    Require(finalized == 0);
    JSGlobalContextRelease(kept);
    Require(finalized == 1);
}

void TestAnotherContextOfTheGroup() {
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef ctx = MakeContext(group);
    theOther = MakeContext(group);
    Reset();
    Require(JSValueToNumber(ctx, Eval(ctx, "act(5) + touched"), nullptr) == 12.0);
    Require(finalizedInsideTheCallback == 0);
    Require(finalized == 1);
    JSGlobalContextRelease(ctx);
    Require(finalized == 2);
    JSContextGroupRelease(group);
}

void TestOutsideACall() {
    JSGlobalContextRef ctx = MakeContext(nullptr);
    Reset();
    Require(JSValueToNumber(ctx, Eval(ctx, "act(0) + touched"), nullptr) == 12.0);
    Require(finalized == 0);
    JSGlobalContextRelease(ctx);
    Require(finalized == 1);
}

}  // namespace

int main() {
    TestTheScriptGoesOn();
    TestTheLastContextOfItsGroup();
    TestThroughACall();
    TestTheOutermostCallDecides();
    TestRetainedAgain();
    TestRetainCounts();
    TestAnotherContextOfTheGroup();
    TestOutsideACall();
    std::puts("libSceNKWebKit JS context released while running tests passed");
    return 0;
}
