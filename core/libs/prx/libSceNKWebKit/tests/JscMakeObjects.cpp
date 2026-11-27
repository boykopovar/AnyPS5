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

constexpr const char* KeepAndReplace =
    "globalThis.RealDate = Date; globalThis.RealError = Error; globalThis.RealRegExp = RegExp;"
    "globalThis.Date = function () { this.fake = true; };"
    "globalThis.Error = function () { this.fake = true; };"
    "globalThis.RegExp = function () { this.fake = true; }; 0";

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JSC make objects check failed at line %u\n", location.line());
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

void RequireRealObjects(JSContextRef ctx, std::source_location location = std::source_location::current()) {
    JSValueRef exception = nullptr;
    JSValueRef day = JSValueMakeNumber(ctx, 86400000.0);
    JSObjectRef date = JSObjectMakeDate(ctx, 1, &day, &exception);
    Require(date != nullptr && exception == nullptr, location);
    Require(JSValueIsDate(ctx, date), location);
    SetGlobal(ctx, "made", date);
    Require(EvalBool(ctx, "made instanceof RealDate && !('fake' in made) && made.getTime() === 86400000"), location);

    JSValueRef errorParts[2] = {Eval(ctx, "'boom'"), Eval(ctx, "({ cause: 7 })")};
    JSObjectRef error = JSObjectMakeError(ctx, 2, errorParts, &exception);
    Require(error != nullptr && exception == nullptr, location);
    SetGlobal(ctx, "made", error);
    Require(EvalBool(ctx, "made instanceof RealError && !('fake' in made) && made.message === 'boom' && made.cause === 7"), location);
    Require(EvalBool(ctx, "Object.prototype.toString.call(made) === '[object Error]'"), location);

    JSValueRef patternParts[2] = {Eval(ctx, "'a+'"), Eval(ctx, "'gi'")};
    JSObjectRef pattern = JSObjectMakeRegExp(ctx, 2, patternParts, &exception);
    Require(pattern != nullptr && exception == nullptr, location);
    SetGlobal(ctx, "made", pattern);
    Require(EvalBool(ctx, "made instanceof RealRegExp && !('fake' in made) && made.source === 'a+' && made.flags === 'gi'"), location);
    Require(EvalBool(ctx, "RealRegExp.prototype.test.call(made, 'xAAy')"), location);
}

void TestUntouchedGlobals() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    Eval(ctx, "globalThis.RealDate = Date; globalThis.RealError = Error; globalThis.RealRegExp = RegExp; 0");
    RequireRealObjects(ctx);

    JSValueRef exception = nullptr;
    JSValueRef fields[3] = {JSValueMakeNumber(ctx, 2020.0), JSValueMakeNumber(ctx, 0.0), JSValueMakeNumber(ctx, 2.0)};
    JSObjectRef date = JSObjectMakeDate(ctx, 3, fields, &exception);
    Require(date != nullptr && exception == nullptr);
    SetGlobal(ctx, "made", date);
    Require(EvalBool(ctx, "made.getFullYear() === 2020 && made.getMonth() === 0 && made.getDate() === 2"));

    Require(JSObjectMakeDate(ctx, 0, nullptr, &exception) != nullptr);
    JSObjectRef emptyError = JSObjectMakeError(ctx, 0, nullptr, &exception);
    Require(emptyError != nullptr && exception == nullptr);
    SetGlobal(ctx, "made", emptyError);
    Require(EvalBool(ctx, "made.message === '' && !Object.prototype.hasOwnProperty.call(made, 'message')"));
    JSObjectRef emptyPattern = JSObjectMakeRegExp(ctx, 0, nullptr, &exception);
    Require(emptyPattern != nullptr && exception == nullptr);
    SetGlobal(ctx, "made", emptyPattern);
    Require(EvalBool(ctx, "made.source === '(?:)'"));

    JSValueRef broken = Eval(ctx, "'('");
    Require(JSObjectMakeRegExp(ctx, 1, &broken, &exception) == nullptr);
    Require(exception != nullptr);
    SetGlobal(ctx, "lastError", exception);
    Require(EvalBool(ctx, "lastError instanceof SyntaxError"));

    JSGlobalContextRelease(ctx);
}

void TestReplacedGlobals() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    Eval(ctx, KeepAndReplace);
    Require(EvalBool(ctx, "new Date().fake === true && new Error().fake === true && new RegExp().fake === true"));
    RequireRealObjects(ctx);

    Eval(ctx, "globalThis.Date = 5; globalThis.Error = null; globalThis.RegExp = {}; 0");
    RequireRealObjects(ctx);

    Eval(ctx, "delete globalThis.Date; delete globalThis.Error; delete globalThis.RegExp; 0");
    Require(EvalBool(ctx, "typeof Date === 'undefined' && typeof Error === 'undefined' && typeof RegExp === 'undefined'"));
    RequireRealObjects(ctx);

    JSGlobalContextRelease(ctx);
}

void TestEachContextUsesItsOwnConstructors() {
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef first = JSGlobalContextCreateInGroup(group, nullptr);
    JSGlobalContextRef second = JSGlobalContextCreateInGroup(group, nullptr);
    Eval(first, KeepAndReplace);
    Eval(second, "globalThis.RealDate = Date; globalThis.RealError = Error; globalThis.RealRegExp = RegExp; 0");
    RequireRealObjects(first);
    RequireRealObjects(second);

    JSValueRef day = JSValueMakeNumber(second, 0.0);
    JSObjectRef date = JSObjectMakeDate(second, 1, &day, nullptr);
    Require(date != nullptr);
    SetGlobal(first, "foreign", date);
    Require(EvalBool(first, "!(foreign instanceof RealDate) && Object.prototype.toString.call(foreign) === '[object Date]'"));

    JSGlobalContextRelease(second);
    JSGlobalContextRelease(first);
    JSContextGroupRelease(group);
}

void APS5_VABI ReplaceOnInitialize(JSContextRef ctx, JSObjectRef object) {
    (void)object;
    Eval(ctx, KeepAndReplace);
}

void TestGlobalClassInitializeRunsAfterTheCapture() {
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.initialize = &ReplaceOnInitialize;
    JSClassRef globalClass = JSClassCreate(&definition);
    Require(globalClass != nullptr);
    JSGlobalContextRef ctx = JSGlobalContextCreate(globalClass);
    Require(EvalBool(ctx, "new Date().fake === true"));
    RequireRealObjects(ctx);
    JSGlobalContextRelease(ctx);
    JSClassRelease(globalClass);
}

}  // namespace

int main() {
    TestUntouchedGlobals();
    TestReplacedGlobals();
    TestEachContextUsesItsOwnConstructors();
    TestGlobalClassInitializeRunsAfterTheCapture();
    std::puts("libSceNKWebKit JS make objects tests passed");
    return 0;
}
