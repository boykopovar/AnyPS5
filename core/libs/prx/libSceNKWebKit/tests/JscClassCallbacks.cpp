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
    std::fprintf(stderr, "JSC class callback check failed at line %u\n", location.line());
    std::abort();
}

struct ScopedJsString {
    JSStringRef ref = nullptr;
    explicit ScopedJsString(const char* utf8) : ref(JSStringCreateWithUTF8CString_nid_postfix(utf8)) {}
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

bool EvalThrows(JSContextRef ctx, const char* code) {
    ScopedJsString script(code);
    JSValueRef exception = nullptr;
    JSEvaluateScript(ctx, script, nullptr, nullptr, 1, &exception);
    return exception != nullptr;
}

void SetGlobal(JSContextRef ctx, const char* name, JSValueRef value) {
    ScopedJsString key(name);
    JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), key, value, kJSPropertyAttributeNone, nullptr);
}

bool HasProperty(JSContextRef ctx, JSObjectRef object, const char* name) {
    ScopedJsString key(name);
    return JSObjectHasProperty(ctx, object, key);
}

JSObjectRef conversionTarget = nullptr;
int declineCalls = 0;
int hasInstanceCalls = 0;
int baseHasPropertyCalls = 0;

bool APS5_VABI CountBaseHasProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName) {
    (void)ctx;
    (void)object;
    (void)propertyName;
    ++baseHasPropertyCalls;
    return false;
}

JSValueRef APS5_VABI ConvertNumbersOnly(JSContextRef ctx, JSObjectRef object, JSType type, JSValueRef* exception) {
    (void)object;
    (void)exception;
    if (type == kJSTypeString) return nullptr;
    return JSValueMakeNumber(ctx, 41.0);
}

JSValueRef APS5_VABI ConvertToTarget(JSContextRef ctx, JSObjectRef object, JSType type, JSValueRef* exception) {
    (void)ctx;
    (void)object;
    (void)type;
    (void)exception;
    return conversionTarget;
}

JSValueRef APS5_VABI ConvertThrows(JSContextRef ctx, JSObjectRef object, JSType type, JSValueRef* exception) {
    (void)object;
    (void)type;
    *exception = JSValueMakeNumber(ctx, 13.0);
    return nullptr;
}

bool APS5_VABI MarkerHasInstance(JSContextRef ctx, JSObjectRef constructor, JSValueRef possibleInstance, JSValueRef* exception) {
    (void)ctx;
    (void)constructor;
    (void)possibleInstance;
    (void)exception;
    ++hasInstanceCalls;
    return true;
}

JSValueRef APS5_VABI ReturnNullCall(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)ctx;
    (void)function;
    (void)thisObject;
    (void)argumentCount;
    (void)arguments;
    (void)exception;
    return nullptr;
}

JSValueRef APS5_VABI GetFive(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception) {
    (void)object;
    (void)propertyName;
    (void)exception;
    return JSValueMakeNumber(ctx, 5.0);
}

JSValueRef APS5_VABI GetNothing(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception) {
    (void)ctx;
    (void)object;
    (void)propertyName;
    (void)exception;
    ++declineCalls;
    return nullptr;
}

JSValueRef APS5_VABI GetThrows(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception) {
    (void)object;
    (void)propertyName;
    *exception = JSValueMakeNumber(ctx, 17.0);
    return nullptr;
}

void TestConvertToType() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);

    JSClassDefinition numbersOnly = kJSClassDefinitionEmpty;
    numbersOnly.className = "Conv";
    numbersOnly.convertToType = &ConvertNumbersOnly;
    JSClassRef numbersOnlyClass = JSClassCreate(&numbersOnly);
    JSClassDefinition derived = kJSClassDefinitionEmpty;
    derived.className = "Sub";
    derived.parentClass = numbersOnlyClass;
    JSClassRef derivedClass = JSClassCreate(&derived);
    Require(numbersOnlyClass != nullptr && derivedClass != nullptr);
    SetGlobal(ctx, "conv", JSObjectMake(ctx, numbersOnlyClass, nullptr));
    SetGlobal(ctx, "sub", JSObjectMake(ctx, derivedClass, nullptr));

    Require(EvalBool(ctx, "+conv === 41 && conv + 1 === 42"));
    Require(EvalBool(ctx, "String(conv) === '[object Conv]'"));
    Require(EvalBool(ctx, "`${conv}` === '[object Conv]'"));
    Require(EvalBool(ctx, "+sub === 41 && String(sub) === '[object Sub]'"));

    JSClassDefinition boxed = kJSClassDefinitionEmpty;
    boxed.className = "Boxed";
    boxed.convertToType = &ConvertToTarget;
    JSClassRef boxedClass = JSClassCreate(&boxed);
    Require(boxedClass != nullptr);
    SetGlobal(ctx, "boxed", JSObjectMake(ctx, boxedClass, nullptr));

    JSValueRef both = Eval(ctx, "var target = { valueOf: function () { return 9; }, toString: function () { return 'nine'; } }; target");
    conversionTarget = JSValueToObject(ctx, both, nullptr);
    Require(conversionTarget != nullptr);
    Require(EvalBool(ctx, "+boxed === 9"));
    Require(EvalBool(ctx, "String(boxed) === 'nine'"));

    JSValueRef skipping = Eval(ctx, "target = { toString: function () { return {}; }, valueOf: function () { return 3; } }; target");
    conversionTarget = JSValueToObject(ctx, skipping, nullptr);
    Require(EvalBool(ctx, "String(boxed) === '3' && +boxed === 3"));

    JSValueRef notCallable = Eval(ctx, "target = { toString: 5, valueOf: function () { return 4; } }; target");
    conversionTarget = JSValueToObject(ctx, notCallable, nullptr);
    Require(EvalBool(ctx, "String(boxed) === '4'"));

    JSValueRef neither = Eval(ctx, "target = { toString: null, valueOf: null }; target");
    conversionTarget = JSValueToObject(ctx, neither, nullptr);
    Require(EvalBool(ctx, "var kind = 'none'; try { String(boxed); } catch (e) { kind = e instanceof TypeError ? 'type' : 'other'; } kind === 'type'"));
    conversionTarget = nullptr;
    Require(EvalBool(ctx, "String(boxed) === '[object Boxed]'"));

    JSClassDefinition thrower = kJSClassDefinitionEmpty;
    thrower.convertToType = &ConvertThrows;
    JSClassRef throwerClass = JSClassCreate(&thrower);
    Require(throwerClass != nullptr);
    SetGlobal(ctx, "thrower", JSObjectMake(ctx, throwerClass, nullptr));
    Require(EvalBool(ctx, "var raised = null; try { String(thrower); } catch (e) { raised = e; } raised === 13"));

    JSClassRelease(throwerClass);
    JSClassRelease(boxedClass);
    JSClassRelease(derivedClass);
    JSClassRelease(numbersOnlyClass);
    JSGlobalContextRelease(ctx);
}

void TestHasInstance() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);

    JSClassDefinition plain = kJSClassDefinitionEmpty;
    plain.className = "Plain";
    JSClassRef plainClass = JSClassCreate(&plain);
    JSClassDefinition callable = kJSClassDefinitionEmpty;
    callable.callAsFunction = &ReturnNullCall;
    JSClassRef callableClass = JSClassCreate(&callable);
    JSClassDefinition marker = kJSClassDefinitionEmpty;
    marker.hasInstance = &MarkerHasInstance;
    JSClassRef markerClass = JSClassCreate(&marker);
    JSClassDefinition inheriting = kJSClassDefinitionEmpty;
    inheriting.parentClass = markerClass;
    JSClassRef inheritingClass = JSClassCreate(&inheriting);
    Require(plainClass != nullptr && callableClass != nullptr && markerClass != nullptr && inheritingClass != nullptr);

    JSObjectRef plainObject = JSObjectMake(ctx, plainClass, nullptr);
    SetGlobal(ctx, "plain", plainObject);
    SetGlobal(ctx, "callable", JSObjectMake(ctx, callableClass, nullptr));
    SetGlobal(ctx, "marked", JSObjectMake(ctx, markerClass, nullptr));
    SetGlobal(ctx, "inheriting", JSObjectMake(ctx, inheritingClass, nullptr));

    hasInstanceCalls = 0;
    Require(!EvalBool(ctx, "({}) instanceof plain"));
    Require(!EvalBool(ctx, "plain instanceof plain"));
    Require(!EvalBool(ctx, "({}) instanceof callable"));
    Require(!EvalBool(ctx, "({}) instanceof Object.getPrototypeOf(plain)"));
    Require(hasInstanceCalls == 0);
    Require(EvalBool(ctx, "({}) instanceof marked"));
    Require(hasInstanceCalls == 1);
    Require(EvalBool(ctx, "({}) instanceof inheriting"));
    Require(hasInstanceCalls == 2);

    JSValueRef exception = nullptr;
    Require(!JSValueIsInstanceOfConstructor(ctx, JSObjectMake(ctx, nullptr, nullptr), plainObject, &exception));
    Require(exception == nullptr);

    JSClassRelease(inheritingClass);
    JSClassRelease(markerClass);
    JSClassRelease(callableClass);
    JSClassRelease(plainClass);
    JSGlobalContextRelease(ctx);
}

void TestStaticValuePresence() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);

    JSStaticValue baseValues[] = {
        {"shared", &GetFive, nullptr, kJSPropertyAttributeNone},
        {nullptr, nullptr, nullptr, 0},
    };
    JSClassDefinition base = kJSClassDefinitionEmpty;
    base.staticValues = baseValues;
    base.hasProperty = &CountBaseHasProperty;
    JSClassRef baseClass = JSClassCreate(&base);

    JSStaticValue values[] = {
        {"present", &GetFive, nullptr, kJSPropertyAttributeNone},
        {"nogetter", nullptr, nullptr, kJSPropertyAttributeNone},
        {"declines", &GetNothing, nullptr, kJSPropertyAttributeNone},
        {"throws", &GetThrows, nullptr, kJSPropertyAttributeNone},
        {"shared", &GetNothing, nullptr, kJSPropertyAttributeNone},
        {nullptr, nullptr, nullptr, 0},
    };
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.parentClass = baseClass;
    definition.staticValues = values;
    JSClassRef valueClass = JSClassCreate(&definition);
    Require(baseClass != nullptr && valueClass != nullptr);

    JSObjectRef object = JSObjectMake(ctx, valueClass, nullptr);
    Require(object != nullptr);
    SetGlobal(ctx, "o", object);

    Require(EvalBool(ctx, "'present' in o && o.present === 5"));
    Require(HasProperty(ctx, object, "present"));
    Require(!EvalBool(ctx, "'nogetter' in o"));
    Require(EvalBool(ctx, "o.nogetter === undefined"));
    Require(!HasProperty(ctx, object, "nogetter"));

    declineCalls = 0;
    Require(!EvalBool(ctx, "'declines' in o"));
    Require(declineCalls >= 1);
    Require(!HasProperty(ctx, object, "declines"));
    Require(EvalBool(ctx, "o.declines === undefined"));

    Require(EvalThrows(ctx, "'throws' in o"));
    Require(EvalBool(ctx, "var raised = null; try { 'throws' in o; } catch (e) { raised = e; } raised === 17"));

    baseHasPropertyCalls = 0;
    Require(EvalBool(ctx, "'shared' in o && o.shared === 5"));
    Require(HasProperty(ctx, object, "shared"));
    Require(baseHasPropertyCalls == 0);
    Require(!EvalBool(ctx, "'absent' in o"));
    Require(baseHasPropertyCalls >= 1);
    Require(EvalBool(ctx, "Object.prototype.hasOwnProperty.call(o, 'shared')"));
    Require(EvalBool(ctx, "Object.prototype.hasOwnProperty.call(o, 'present')"));
    Require(!EvalBool(ctx, "Object.prototype.hasOwnProperty.call(o, 'declines')"));

    Require(EvalBool(ctx, "o.declines = 4; o.declines === 4 && 'declines' in o"));
    Require(HasProperty(ctx, object, "declines"));
    Require(!EvalBool(ctx, "'absent' in o"));

    JSClassRelease(valueClass);
    JSClassRelease(baseClass);
    JSGlobalContextRelease(ctx);
}

void TestCallbackReturningNull() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);

    JSStaticFunction functions[] = {
        {"nothing", &ReturnNullCall, kJSPropertyAttributeNone},
        {nullptr, nullptr, 0},
    };
    JSClassDefinition callable = kJSClassDefinitionEmpty;
    callable.callAsFunction = &ReturnNullCall;
    callable.staticFunctions = functions;
    JSClassRef callableClass = JSClassCreate(&callable);
    Require(callableClass != nullptr);
    JSObjectRef object = JSObjectMake(ctx, callableClass, nullptr);
    Require(object != nullptr);
    SetGlobal(ctx, "callable", object);

    Require(EvalBool(ctx, "callable() === null"));
    JSValueRef exception = nullptr;
    JSValueRef result = JSObjectCallAsFunction(ctx, object, nullptr, 0, nullptr, &exception);
    Require(result != nullptr && JSValueIsNull(ctx, result) && exception == nullptr);

    Require(EvalBool(ctx, "callable.nothing() === undefined"));
    SetGlobal(ctx, "direct", JSObjectMakeFunctionWithCallback(ctx, nullptr, &ReturnNullCall));
    Require(EvalBool(ctx, "direct() === undefined"));

    JSClassRelease(callableClass);
    JSGlobalContextRelease(ctx);
}

}

int main() {
    TestConvertToType();
    TestHasInstance();
    TestStaticValuePresence();
    TestCallbackReturningNull();
    std::puts("libSceNKWebKit JSC class callback tests passed");
    return 0;
}
