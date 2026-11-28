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
    std::fprintf(stderr, "JSC property attributes check failed at line %u\n", location.line());
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

int setPropertyCalls = 0;
int staticSetterCalls = 0;

bool APS5_VABI HasDynamic(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName) {
    (void)ctx;
    (void)object;
    return JSStringIsEqualToUTF8CString(propertyName, "dynamic");
}

JSValueRef APS5_VABI GetDynamic(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception) {
    (void)object;
    (void)exception;
    if (!JSStringIsEqualToUTF8CString(propertyName, "dynamic")) return nullptr;
    return JSValueMakeNumber(ctx, 11.0);
}

bool APS5_VABI CountSetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception) {
    (void)ctx;
    (void)object;
    (void)value;
    (void)exception;
    if (!JSStringIsEqualToUTF8CString(propertyName, "dynamic")) return false;
    ++setPropertyCalls;
    return true;
}

void APS5_VABI ListDynamic(JSContextRef ctx, JSObjectRef object, JSPropertyNameAccumulatorRef names) {
    (void)ctx;
    (void)object;
    ScopedJsString name("dynamic");
    JSPropertyNameAccumulatorAddName(names, name);
}

JSValueRef APS5_VABI StaticGetter(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception) {
    (void)object;
    (void)propertyName;
    (void)exception;
    return JSValueMakeNumber(ctx, 22.0);
}

bool APS5_VABI StaticSetter(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception) {
    (void)ctx;
    (void)object;
    (void)propertyName;
    (void)value;
    (void)exception;
    ++staticSetterCalls;
    return true;
}

const JSStaticValue staticValues[] = {
    {"plain", &StaticGetter, &StaticSetter, kJSPropertyAttributeNone},
    {"readOnly", &StaticGetter, nullptr, kJSPropertyAttributeReadOnly},
    {"keep", &StaticGetter, nullptr, kJSPropertyAttributeDontDelete},
    {"hidden", &StaticGetter, nullptr, kJSPropertyAttributeDontEnum},
    {"everything", &StaticGetter, nullptr, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete | kJSPropertyAttributeDontEnum},
    {nullptr, nullptr, nullptr, 0},
};

const char* const Helpers =
    "function attributes(object, name) { var d = Object.getOwnPropertyDescriptor(object, name);"
    "  return d === undefined ? 'missing' : [typeof d.get === 'undefined' && typeof d.set === 'undefined' ? 'data' : 'accessor', d.value, d.writable, d.configurable].join(); }"
    "function listed(object) { var names = []; for (var name in object) names.push(name); return names.sort().join(); } true";

void TestCallbackProperties(bool withHasProperty) {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    if (withHasProperty) definition.hasProperty = &HasDynamic;
    definition.getProperty = &GetDynamic;
    definition.setProperty = &CountSetProperty;
    definition.getPropertyNames = &ListDynamic;
    JSClassRef jsClass = JSClassCreate(&definition);
    SetGlobal(ctx, "target", JSObjectMake(ctx, jsClass, nullptr));
    Require(EvalBool(ctx, Helpers));

    Require(EvalBool(ctx, "attributes(target, 'dynamic') === 'data,11,false,true'"));
    Require(EvalBool(ctx, "attributes(target, 'absent') === 'missing'"));
    Require(EvalBool(ctx, "listed(target) === 'dynamic' && Object.keys(target).join() === 'dynamic'"));
    setPropertyCalls = 0;
    Require(EvalBool(ctx, "target.dynamic = 5; target.dynamic === 11"));
    Require(setPropertyCalls == 1);
    Require(EvalBool(ctx, "(function () { 'use strict'; target.dynamic = 6; return target.dynamic === 11; })()"));
    Require(setPropertyCalls == 2);
    Require(EvalBool(ctx, "!Object.isFrozen(target) && !Object.isSealed(target)"));
    JSClassRelease(jsClass);
    JSGlobalContextRelease(ctx);
}

void TestStaticValues() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.staticValues = staticValues;
    JSClassRef jsClass = JSClassCreate(&definition);
    SetGlobal(ctx, "target", JSObjectMake(ctx, jsClass, nullptr));
    Require(EvalBool(ctx, Helpers));

    Require(EvalBool(ctx, "['plain', 'readOnly', 'keep', 'hidden', 'everything'].every(function (name) { return attributes(target, name) === 'data,22,false,true'; })"));
    Require(EvalBool(ctx, "Object.getOwnPropertyDescriptor(target, 'hidden').enumerable === false && Object.getOwnPropertyDescriptor(target, 'everything').enumerable === false"));
    Require(EvalBool(ctx, "listed(target) === 'keep,plain,readOnly' && Object.keys(target).sort().join() === 'keep,plain,readOnly'"));

    staticSetterCalls = 0;
    Require(EvalBool(ctx, "target.plain = 1; target.plain === 22"));
    Require(staticSetterCalls == 1);
    Require(EvalBool(ctx, "(function () { 'use strict'; target.plain = 2; return target.plain === 22; })()"));
    Require(staticSetterCalls == 2);
    Require(EvalBool(ctx, "target.readOnly = 3; target.readOnly === 22"));
    Require(EvalBool(ctx, "delete target.keep === false && target.keep === 22 && delete target.everything === false"));
    Require(EvalBool(ctx, "attributes(target, 'keep') === 'data,22,false,true'"));
    JSClassRelease(jsClass);
    JSGlobalContextRelease(ctx);
}

}  // namespace

int main() {
    TestCallbackProperties(true);
    TestCallbackProperties(false);
    TestStaticValues();
    std::puts("libSceNKWebKit JS property attributes tests passed");
    return 0;
}
