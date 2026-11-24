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
    std::fprintf(stderr, "JSC global prototype check failed at line %u\n", location.line());
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

JSValueRef APS5_VABI ReturnFive(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)function;
    (void)thisObject;
    (void)argumentCount;
    (void)arguments;
    (void)exception;
    return JSValueMakeNumber(ctx, 5.0);
}

const JSStaticFunction globalFunctions[] = {
    {"classFunction", &ReturnFive, kJSPropertyAttributeNone},
    {nullptr, nullptr, 0},
};

JSObjectRef MakeMarked(JSContextRef ctx) {
    JSObjectRef object = JSObjectMake(ctx, nullptr, nullptr);
    ScopedJsString key("fromAbove");
    JSObjectSetProperty(ctx, object, key, JSValueMakeNumber(ctx, 1.0), kJSPropertyAttributeNone, nullptr);
    JSValueProtect(ctx, object);
    return object;
}

void RequireUnchanged(JSContextRef ctx, JSObjectRef global, JSValueRef original, std::source_location location = std::source_location::current()) {
    Require(JSValueIsStrictEqual(ctx, JSObjectGetPrototype(ctx, global), original), location);
    Require(EvalBool(ctx, "typeof fromAbove === 'undefined' && !('fromAbove' in this)", location), location);
}

void TestAGlobalObject(JSClassRef globalClass) {
    JSGlobalContextRef ctx = JSGlobalContextCreate(globalClass);
    JSObjectRef global = JSContextGetGlobalObject(ctx);
    JSValueRef original = JSObjectGetPrototype(ctx, global);
    Require(original != nullptr && JSValueIsObject(ctx, original));
    JSValueProtect(ctx, original);
    JSObjectRef above = MakeMarked(ctx);

    JSObjectSetPrototype(ctx, global, above);
    RequireUnchanged(ctx, global, original);
    JSObjectSetPrototype(ctx, global, JSValueMakeNull(ctx));
    RequireUnchanged(ctx, global, original);
    JSObjectSetPrototype(ctx, global, nullptr);
    RequireUnchanged(ctx, global, original);
    JSObjectSetPrototype(ctx, global, JSValueMakeNumber(ctx, 3.0));
    RequireUnchanged(ctx, global, original);
    JSObjectSetPrototype(ctx, global, original);
    RequireUnchanged(ctx, global, original);
    Require(EvalBool(ctx, "typeof Object.prototype.hasOwnProperty === 'function' && this.toString === Object.prototype.toString"));
    if (globalClass) Require(EvalBool(ctx, "classFunction() === 5"));

    JSGlobalContextRef other = JSGlobalContextCreateInGroup(JSContextGetGroup(ctx), nullptr);
    JSObjectSetPrototype(other, global, above);
    RequireUnchanged(ctx, global, original);
    JSObjectRef otherGlobal = JSContextGetGlobalObject(other);
    JSValueRef otherOriginal = JSObjectGetPrototype(other, otherGlobal);
    JSValueProtect(other, otherOriginal);
    JSObjectSetPrototype(ctx, otherGlobal, above);
    RequireUnchanged(other, otherGlobal, otherOriginal);
    JSValueUnprotect(other, otherOriginal);
    JSGlobalContextRelease(other);

    JSValueUnprotect(ctx, above);
    JSValueUnprotect(ctx, original);
    JSGlobalContextRelease(ctx);
}

void TestOtherObjectsStillChange() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    JSObjectRef above = MakeMarked(ctx);
    JSObjectRef object = JSObjectMake(ctx, nullptr, nullptr);
    JSValueProtect(ctx, object);
    JSObjectSetPrototype(ctx, object, above);
    Require(JSValueIsStrictEqual(ctx, JSObjectGetPrototype(ctx, object), above));
    ScopedJsString key("fromAbove");
    Require(JSObjectHasProperty(ctx, object, key));
    JSObjectSetPrototype(ctx, object, JSValueMakeNull(ctx));
    Require(JSValueIsNull(ctx, JSObjectGetPrototype(ctx, object)));
    JSObjectSetPrototype(ctx, object, above);
    JSObjectSetPrototype(ctx, object, JSValueMakeNumber(ctx, 3.0));
    Require(JSValueIsNull(ctx, JSObjectGetPrototype(ctx, object)));

    JSClassDefinition definition = kJSClassDefinitionEmpty;
    JSClassRef jsClass = JSClassCreate(&definition);
    JSObjectRef classObject = JSObjectMake(ctx, jsClass, nullptr);
    JSValueProtect(ctx, classObject);
    JSObjectSetPrototype(ctx, classObject, above);
    Require(JSValueIsStrictEqual(ctx, JSObjectGetPrototype(ctx, classObject), above));
    JSValueUnprotect(ctx, classObject);
    JSClassRelease(jsClass);

    JSValueUnprotect(ctx, object);
    JSValueUnprotect(ctx, above);
    JSGlobalContextRelease(ctx);
}

}  // namespace

int main() {
    TestAGlobalObject(nullptr);
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.className = "Global";
    definition.staticFunctions = globalFunctions;
    JSClassRef globalClass = JSClassCreate(&definition);
    TestAGlobalObject(globalClass);
    JSClassRelease(globalClass);
    TestOtherObjectsStillChange();
    std::puts("libSceNKWebKit JS global prototype tests passed");
    return 0;
}
