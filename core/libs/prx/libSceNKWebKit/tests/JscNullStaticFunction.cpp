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
    std::fprintf(stderr, "JSC null static function check failed at line %u\n", location.line());
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

JSValueRef APS5_VABI ReturnOne(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)function;
    (void)thisObject;
    (void)argumentCount;
    (void)arguments;
    (void)exception;
    return JSValueMakeNumber(ctx, 1.0);
}

JSValueRef APS5_VABI ReturnTwo(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)function;
    (void)thisObject;
    (void)argumentCount;
    (void)arguments;
    (void)exception;
    return JSValueMakeNumber(ctx, 2.0);
}

const JSStaticFunction ownFunctions[] = {
    {"works", &ReturnOne, kJSPropertyAttributeNone},
    {"nullCall", nullptr, kJSPropertyAttributeNone},
    {"hiddenNull", nullptr, kJSPropertyAttributeDontEnum},
    {nullptr, nullptr, 0},
};

const JSStaticFunction parentFunctions[] = {
    {"inherited", &ReturnTwo, kJSPropertyAttributeDontEnum},
    {"nullInBoth", nullptr, kJSPropertyAttributeNone},
    {"both", &ReturnTwo, kJSPropertyAttributeNone},
    {nullptr, nullptr, 0},
};

const JSStaticFunction childFunctions[] = {
    {"inherited", nullptr, kJSPropertyAttributeNone},
    {"nullInBoth", nullptr, kJSPropertyAttributeNone},
    {"both", &ReturnOne, kJSPropertyAttributeNone},
    {nullptr, nullptr, 0},
};

const char* const ThrowsReferenceError = "(function (read) { try { read(); } catch (error) { return error instanceof ReferenceError; } return false; })";

bool ReadThrowsReferenceError(JSContextRef ctx, const char* expression, std::source_location location = std::source_location::current()) {
    char code[512];
    std::snprintf(code, sizeof(code), "%s(function () { return %s; })", ThrowsReferenceError, expression);
    return EvalBool(ctx, code, location);
}

bool CopiedNamesContain(JSContextRef ctx, JSObjectRef object, const char* name) {
    JSPropertyNameArrayRef names = JSObjectCopyPropertyNames(ctx, object);
    Require(names != nullptr);
    bool found = false;
    const std::size_t count = JSPropertyNameArrayGetCount(names);
    for (std::size_t index = 0; index < count; ++index) {
        if (JSStringIsEqualToUTF8CString(JSPropertyNameArrayGetNameAtIndex(names, index), name)) found = true;
    }
    JSPropertyNameArrayRelease(names);
    return found;
}

void TestAFunctionOfTheObject() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.attributes = kJSClassAttributeNoAutomaticPrototype;
    definition.staticFunctions = ownFunctions;
    JSClassRef jsClass = JSClassCreate(&definition);
    JSObjectRef object = JSObjectMake(ctx, jsClass, nullptr);
    SetGlobal(ctx, "target", object);

    Require(EvalBool(ctx, "'nullCall' in target && Object.prototype.hasOwnProperty.call(target, 'nullCall') && 'hiddenNull' in target"));
    ScopedJsString nullCall("nullCall");
    Require(JSObjectHasProperty(ctx, object, nullCall));

    Require(EvalBool(ctx, "var seen = []; for (var name in target) seen.push(name); seen.sort().join() === 'nullCall,works'"));
    Require(EvalBool(ctx, "Object.keys(target).sort().join() === 'nullCall,works'"));
    Require(CopiedNamesContain(ctx, object, "nullCall"));
    Require(CopiedNamesContain(ctx, object, "works"));
    Require(!CopiedNamesContain(ctx, object, "hiddenNull"));

    Require(ReadThrowsReferenceError(ctx, "target.nullCall"));
    Require(ReadThrowsReferenceError(ctx, "target.nullCall()"));
    Require(ReadThrowsReferenceError(ctx, "target.hiddenNull"));
    Require(ReadThrowsReferenceError(ctx, "Object.assign({}, target)"));
    JSValueRef exception = nullptr;
    JSValueRef value = JSObjectGetProperty(ctx, object, nullCall, &exception);
    Require(exception != nullptr);
    Require(value == nullptr || JSValueIsUndefined(ctx, value));
    SetGlobal(ctx, "apiError", exception);
    Require(EvalBool(ctx, "apiError instanceof ReferenceError"));

    Require(EvalBool(ctx, "target.works() === 1"));
    Require(EvalBool(ctx, "target.nullCall = 5; target.nullCall === 5"));
    Require(EvalBool(ctx, "seen = []; for (name in target) seen.push(name); seen.sort().join() === 'nullCall,works'"));
    JSClassRelease(jsClass);
    JSGlobalContextRelease(ctx);
}

void TestAParentClassSuppliesTheFunction() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    JSClassDefinition parentDefinition = kJSClassDefinitionEmpty;
    parentDefinition.attributes = kJSClassAttributeNoAutomaticPrototype;
    parentDefinition.staticFunctions = parentFunctions;
    JSClassRef parentClass = JSClassCreate(&parentDefinition);
    JSClassDefinition childDefinition = kJSClassDefinitionEmpty;
    childDefinition.attributes = kJSClassAttributeNoAutomaticPrototype;
    childDefinition.parentClass = parentClass;
    childDefinition.staticFunctions = childFunctions;
    JSClassRef childClass = JSClassCreate(&childDefinition);
    SetGlobal(ctx, "child", JSObjectMake(ctx, childClass, nullptr));

    Require(EvalBool(ctx, "var names = []; for (var name in child) names.push(name); names.sort().join() === 'both,inherited,nullInBoth'"));
    Require(EvalBool(ctx, "typeof child.inherited === 'function' && child.inherited() === 2"));
    Require(EvalBool(ctx, "child.both() === 1"));
    Require(EvalBool(ctx, "child.inherited === child.inherited"));
    Require(ReadThrowsReferenceError(ctx, "child.nullInBoth"));
    Require(EvalBool(ctx, "names = []; for (name in child) names.push(name); names.sort().join() === 'both,inherited,nullInBoth'"));
    JSClassRelease(childClass);
    JSClassRelease(parentClass);
    JSGlobalContextRelease(ctx);
}

void TestAFunctionOfThePrototype() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.staticFunctions = ownFunctions;
    JSClassRef jsClass = JSClassCreate(&definition);
    SetGlobal(ctx, "instance", JSObjectMake(ctx, jsClass, nullptr));

    Require(EvalBool(ctx, "'nullCall' in instance && !Object.prototype.hasOwnProperty.call(instance, 'nullCall')"));
    Require(EvalBool(ctx, "var seen = []; for (var name in instance) seen.push(name); seen.sort().join() === 'nullCall,works'"));
    Require(ReadThrowsReferenceError(ctx, "instance.nullCall"));
    Require(ReadThrowsReferenceError(ctx, "instance.hiddenNull"));
    Require(EvalBool(ctx, "instance.works() === 1"));
    JSClassRelease(jsClass);
    JSGlobalContextRelease(ctx);
}

void TestAFunctionOfTheGlobalObject() {
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.staticFunctions = ownFunctions;
    JSClassRef jsClass = JSClassCreate(&definition);
    JSGlobalContextRef ctx = JSGlobalContextCreate(jsClass);

    Require(EvalBool(ctx, "'nullCall' in this"));
    Require(EvalBool(ctx, "var seen = []; for (var name in this) seen.push(name); seen.indexOf('nullCall') >= 0 && seen.indexOf('works') >= 0 && seen.indexOf('hiddenNull') < 0"));
    Require(ReadThrowsReferenceError(ctx, "this.nullCall"));
    Require(EvalBool(ctx, "works() === 1"));
    JSGlobalContextRelease(ctx);
    JSClassRelease(jsClass);
}

}  // namespace

int main() {
    TestAFunctionOfTheObject();
    TestAParentClassSuppliesTheFunction();
    TestAFunctionOfThePrototype();
    TestAFunctionOfTheGlobalObject();
    std::puts("libSceNKWebKit JS null static function tests passed");
    return 0;
}
