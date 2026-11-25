#include "../src/Jsc.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceJsc/include/Api.hpp"
#include "prx/libSceJsc/include/ApiBridgeExports.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>

using namespace NKWebKit;

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JSC object outlives context check failed at line %u\n", location.line());
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

JSValueRef MakeText(JSContextRef ctx, const char* utf8) {
    ScopedJsString text(utf8);
    return static_cast<JSValueRef>(JSValueMakeString(ctx, text));
}

int setCalls = 0;
int deleteCalls = 0;

bool APS5_VABI HasName(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName) {
    (void)object;
    return JSStringIsEqualToUTF8CString(propertyName, "name") && JSValueIsString(ctx, MakeText(ctx, "probe"));
}

JSValueRef APS5_VABI GetName(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception) {
    (void)object;
    (void)exception;
    if (!JSStringIsEqualToUTF8CString(propertyName, "name")) return nullptr;
    return MakeText(ctx, "value");
}

bool APS5_VABI SetName(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception) {
    (void)object;
    (void)exception;
    if (!JSStringIsEqualToUTF8CString(propertyName, "name")) return false;
    if (JSValueIsString(ctx, MakeText(ctx, "probe")) && JSValueToNumber(ctx, value, nullptr) == 4.0) ++setCalls;
    return true;
}

bool APS5_VABI DeleteName(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception) {
    (void)object;
    (void)exception;
    if (!JSStringIsEqualToUTF8CString(propertyName, "name")) return false;
    if (JSValueIsString(ctx, MakeText(ctx, "probe"))) ++deleteCalls;
    return true;
}

void APS5_VABI ListName(JSContextRef ctx, JSObjectRef object, JSPropertyNameAccumulatorRef names) {
    (void)object;
    if (!JSValueIsString(ctx, MakeText(ctx, "probe"))) return;
    ScopedJsString name("name");
    JSPropertyNameAccumulatorAddName(names, name);
}

JSValueRef APS5_VABI CallReturnsText(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)function;
    (void)thisObject;
    (void)argumentCount;
    (void)arguments;
    (void)exception;
    return MakeText(ctx, "called");
}

JSValueRef APS5_VABI ConvertToText(JSContextRef ctx, JSObjectRef object, JSType type, JSValueRef* exception) {
    (void)object;
    (void)exception;
    if (type == kJSTypeString) return MakeText(ctx, "converted");
    return JSValueMakeNumber(ctx, 8.0);
}

bool APS5_VABI HasInstanceOfText(JSContextRef ctx, JSObjectRef constructor, JSValueRef possibleInstance, JSValueRef* exception) {
    (void)constructor;
    (void)exception;
    return JSValueIsString(ctx, MakeText(ctx, "probe")) && JSValueIsNumber(ctx, possibleInstance);
}

JSObjectRef APS5_VABI ConstructPlain(JSContextRef ctx, JSObjectRef constructor, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)constructor;
    (void)argumentCount;
    (void)arguments;
    (void)exception;
    JSObjectRef made = JSObjectMake(ctx, nullptr, nullptr);
    ScopedJsString key("madeBy");
    JSObjectSetProperty(ctx, made, key, MakeText(ctx, "constructor"), kJSPropertyAttributeNone, nullptr);
    return made;
}

const JSStaticFunction staticFunctions[] = {
    {"method", &CallReturnsText, kJSPropertyAttributeNone},
    {nullptr, nullptr, 0},
};

JSClassRef MakeClass() {
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.hasProperty = &HasName;
    definition.getProperty = &GetName;
    definition.setProperty = &SetName;
    definition.deleteProperty = &DeleteName;
    definition.getPropertyNames = &ListName;
    definition.callAsFunction = &CallReturnsText;
    definition.convertToType = &ConvertToText;
    definition.hasInstance = &HasInstanceOfText;
    definition.staticFunctions = staticFunctions;
    JSClassRef jsClass = JSClassCreate(&definition);
    Require(jsClass != nullptr);
    return jsClass;
}

void RequireTheObjectWorks(JSContextRef ctx, JSObjectRef object) {
    Require(EvalBool(ctx, "survivor.name === 'value' && 'name' in survivor"));
    setCalls = 0;
    deleteCalls = 0;
    Require(EvalBool(ctx, "survivor.name = 4; delete survivor.name"));
    Require(setCalls == 1 && deleteCalls == 1);
    Require(EvalBool(ctx, "var names = []; for (var key in survivor) names.push(key); names.indexOf('name') >= 0"));
    Require(EvalBool(ctx, "survivor() === 'called' && survivor.method() === 'called'"));
    Require(EvalBool(ctx, "String(survivor) === 'converted' && survivor * 2 === 16 && (3 instanceof survivor)"));

    ScopedJsString name("name");
    JSValueRef exception = nullptr;
    JSValueRef value = JSObjectGetProperty(ctx, object, name, &exception);
    Require(exception == nullptr && value != nullptr && JSValueIsString(ctx, value));
    JSGlobalContextRef owner = JSObjectGetGlobalContext(object);
    Require(owner != nullptr);
    Require(EvalBool(owner, "1 + 1 === 2"));
    Require(JSValueIsStrictEqual(nullptr, object, Eval(ctx, "survivor")));
}

void TestOneSurvivor(bool collect) {
    JSClassRef jsClass = MakeClass();
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef first = JSGlobalContextCreateInGroup(group, nullptr);
    JSGlobalContextRef second = JSGlobalContextCreateInGroup(group, nullptr);
    JSObjectRef object = JSObjectMake(first, jsClass, nullptr);
    JSValueProtect(first, object);
    SetGlobal(second, "survivor", object);
    Require(EvalBool(first, "this.plainOfFirst = 12; true"));
    SetGlobal(second, "firstGlobal", JSContextGetGlobalObject(first));
    SetGlobal(second, "apiFunction", JSObjectMakeFunctionWithCallback(first, nullptr, &CallReturnsText));
    SetGlobal(second, "ApiConstructor", JSObjectMakeConstructor(first, nullptr, &ConstructPlain));
    RequireTheObjectWorks(second, object);
    Require(EvalBool(second, "apiFunction() === 'called' && new ApiConstructor().madeBy === 'constructor'"));

    JSGlobalContextRelease(first);
    if (collect) JSGarbageCollect(second);
    RequireTheObjectWorks(second, object);
    Require(EvalBool(second, "firstGlobal.plainOfFirst === 12"));
    Require(EvalBool(second, "apiFunction() === 'called' && new ApiConstructor().madeBy === 'constructor'"));

    JSValueUnprotect(second, object);
    JSGlobalContextRelease(second);
    JSContextGroupRelease(group);
    JSClassRelease(jsClass);
}

void TestTheObjectIsHandedOnTwice() {
    JSClassRef jsClass = MakeClass();
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef first = JSGlobalContextCreateInGroup(group, nullptr);
    JSGlobalContextRef second = JSGlobalContextCreateInGroup(group, nullptr);
    JSGlobalContextRef third = JSGlobalContextCreateInGroup(group, nullptr);
    JSObjectRef object = JSObjectMake(first, jsClass, nullptr);
    JSValueProtect(first, object);
    SetGlobal(third, "survivor", object);
    JSGlobalContextRelease(first);
    RequireTheObjectWorks(third, object);
    JSGlobalContextRelease(second);
    JSGarbageCollect(third);
    RequireTheObjectWorks(third, object);
    JSValueUnprotect(third, object);
    JSGlobalContextRelease(third);
    JSContextGroupRelease(group);
    JSClassRelease(jsClass);
}

void TestNoContextIsLeft() {
    JSClassRef jsClass = MakeClass();
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef only = JSGlobalContextCreateInGroup(group, nullptr);
    JSObjectRef object = JSObjectMake(only, jsClass, nullptr);
    JSValueProtect(only, object);
    JSGlobalContextRelease(only);
    Require(JSObjectGetGlobalContext(object) == nullptr);
    JSGlobalContextRef later = JSGlobalContextCreateInGroup(group, nullptr);
    Require(JSObjectGetGlobalContext(object) == later);
    SetGlobal(later, "survivor", object);
    RequireTheObjectWorks(later, object);
    JSValueUnprotect(later, object);
    JSGlobalContextRelease(later);
    JSContextGroupRelease(group);
    JSClassRelease(jsClass);
}

}  // namespace

int main() {
    TestOneSurvivor(false);
    TestOneSurvivor(true);
    TestTheObjectIsHandedOnTwice();
    TestNoContextIsLeft();
    std::puts("libSceNKWebKit JS object outlives context tests passed");
    return 0;
}
