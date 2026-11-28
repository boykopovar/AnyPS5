#include "../src/Jsc.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceJsc/include/Api.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <source_location>

using namespace NKWebKit;

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JSC class prototype check failed at line %u\n", location.line());
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

bool EvalBool(JSContextRef ctx, const char* code, std::source_location location = std::source_location::current()) {
    ScopedJsString script(code);
    JSValueRef exception = nullptr;
    JSValueRef value = JSEvaluateScript(ctx, script, nullptr, nullptr, 1, &exception);
    Require(exception == nullptr && value != nullptr, location);
    return JSValueToBoolean(ctx, value);
}

void SetGlobal(JSContextRef ctx, const char* name, JSValueRef value) {
    ScopedJsString key(name);
    JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), key, value, kJSPropertyAttributeNone, nullptr);
}

JSValueRef APS5_VABI Twice(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)function;
    (void)thisObject;
    (void)exception;
    return JSValueMakeNumber(ctx, argumentCount > 0 ? 2.0 * JSValueToNumber(ctx, arguments[0], nullptr) : 0.0);
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

JSValueRef APS5_VABI GetFive(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception) {
    (void)object;
    (void)propertyName;
    (void)exception;
    return JSValueMakeNumber(ctx, 5.0);
}

JSValueRef APS5_VABI GetOne(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception) {
    (void)object;
    (void)propertyName;
    (void)exception;
    return JSValueMakeNumber(ctx, 1.0);
}

JSValueRef APS5_VABI GetTwo(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception) {
    (void)object;
    (void)propertyName;
    (void)exception;
    return JSValueMakeNumber(ctx, 2.0);
}

bool HasName(JSContextRef ctx, JSObjectRef object, const char* expected) {
    JSPropertyNameArrayRef names = JSObjectCopyPropertyNames(ctx, object);
    Require(names != nullptr);
    bool found = false;
    const std::size_t count = JSPropertyNameArrayGetCount(names);
    for (std::size_t index = 0; index < count; ++index) {
        if (JSStringIsEqualToUTF8CString(JSPropertyNameArrayGetNameAtIndex(names, index), expected)) found = true;
    }
    JSPropertyNameArrayRelease(names);
    return found;
}

void TestStaticFunctionsOnPrototype() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);

    JSStaticValue values[] = {
        {"level", &GetFive, nullptr, kJSPropertyAttributeNone},
        {nullptr, nullptr, nullptr, 0},
    };
    JSStaticFunction functions[] = {
        {"twice", &Twice, kJSPropertyAttributeNone},
        {nullptr, nullptr, 0},
    };
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.className = "Tool";
    definition.staticValues = values;
    definition.staticFunctions = functions;
    JSClassRef toolClass = JSClassCreate(&definition);
    Require(toolClass != nullptr);
    JSObjectRef tool = JSObjectMake(ctx, toolClass, nullptr);
    Require(tool != nullptr);
    SetGlobal(ctx, "tool", tool);

    Require(EvalBool(ctx, "tool.twice(4) === 8 && 'twice' in tool"));
    Require(!EvalBool(ctx, "Object.prototype.hasOwnProperty.call(tool, 'twice')"));
    Require(EvalBool(ctx, "Object.keys(tool).indexOf('twice') === -1"));
    Require(EvalBool(ctx, "Object.prototype.hasOwnProperty.call(Object.getPrototypeOf(tool), 'twice')"));
    Require(EvalBool(ctx, "tool.twice === Object.getPrototypeOf(tool).twice"));
    Require(EvalBool(ctx, "Object.prototype.hasOwnProperty.call(tool, 'level') && tool.level === 5"));
    Require(!EvalBool(ctx, "Object.prototype.hasOwnProperty.call(Object.getPrototypeOf(tool), 'level')"));
    Require(HasName(ctx, tool, "twice") && HasName(ctx, tool, "level"));
    Require(EvalBool(ctx, "Object.prototype.toString.call(tool) === '[object Tool]'"));
    Require(EvalBool(ctx, "Object.prototype.toString.call(Object.getPrototypeOf(tool)) === '[object CallbackObject]'"));

    JSClassDefinition bare = kJSClassDefinitionEmpty;
    bare.className = "Bare";
    JSClassRef bareClass = JSClassCreate(&bare);
    Require(bareClass != nullptr);
    SetGlobal(ctx, "bare", JSObjectMake(ctx, bareClass, nullptr));
    Require(EvalBool(ctx, "Object.getPrototypeOf(bare) !== Object.prototype && Object.getPrototypeOf(Object.getPrototypeOf(bare)) === Object.prototype"));
    Require(EvalBool(ctx, "Object.prototype.toString.call(Object.getPrototypeOf(bare)) === '[object CallbackObject]'"));

    JSClassRelease(bareClass);
    JSClassRelease(toolClass);
    JSGlobalContextRelease(ctx);
}

void TestNoAutomaticPrototype() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);

    JSStaticValue baseValues[] = {
        {"level", &GetFive, nullptr, kJSPropertyAttributeNone},
        {nullptr, nullptr, nullptr, 0},
    };
    JSStaticFunction baseFunctions[] = {
        {"twice", &Twice, kJSPropertyAttributeNone},
        {nullptr, nullptr, 0},
    };
    JSClassDefinition base = kJSClassDefinitionEmpty;
    base.className = "Base";
    base.staticValues = baseValues;
    base.staticFunctions = baseFunctions;
    JSClassRef baseClass = JSClassCreate(&base);
    Require(baseClass != nullptr);

    JSStaticFunction childFunctions[] = {
        {"own", &ReturnOne, kJSPropertyAttributeNone},
        {nullptr, nullptr, 0},
    };
    JSClassDefinition child = kJSClassDefinitionEmpty;
    child.className = "Child";
    child.attributes = kJSClassAttributeNoAutomaticPrototype;
    child.parentClass = baseClass;
    child.staticFunctions = childFunctions;
    JSClassRef childClass = JSClassCreate(&child);
    Require(childClass != nullptr);
    SetGlobal(ctx, "kid", JSObjectMake(ctx, childClass, nullptr));
    SetGlobal(ctx, "baseObject", JSObjectMake(ctx, baseClass, nullptr));

    Require(EvalBool(ctx, "Object.getPrototypeOf(kid) === Object.prototype"));
    Require(EvalBool(ctx, "Object.getPrototypeOf(baseObject) !== Object.prototype && baseObject.twice(3) === 6"));
    Require(EvalBool(ctx, "kid.own() === 1 && Object.prototype.hasOwnProperty.call(kid, 'own')"));
    Require(EvalBool(ctx, "typeof kid.twice === 'undefined' && !('twice' in kid)"));
    Require(EvalBool(ctx, "kid.level === 5"));

    JSClassDefinition grandchild = kJSClassDefinitionEmpty;
    grandchild.className = "Grandchild";
    grandchild.parentClass = childClass;
    JSClassRef grandchildClass = JSClassCreate(&grandchild);
    Require(grandchildClass != nullptr);
    SetGlobal(ctx, "grandkid", JSObjectMake(ctx, grandchildClass, nullptr));
    Require(EvalBool(ctx, "Object.getPrototypeOf(grandkid) !== Object.prototype && Object.getPrototypeOf(Object.getPrototypeOf(grandkid)) === Object.prototype"));
    Require(EvalBool(ctx, "grandkid.own() === 1 && grandkid.level === 5"));

    SetGlobal(ctx, "KidConstructor", JSObjectMakeConstructor(ctx, childClass, nullptr));
    Require(EvalBool(ctx, "KidConstructor.prototype === Object.prototype"));

    JSClassRelease(grandchildClass);
    JSClassRelease(childClass);
    JSClassRelease(baseClass);
    JSGlobalContextRelease(ctx);
}

void TestVersionAndDuplicateNames() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);

    JSStaticValue values[] = {
        {"dup", &GetOne, nullptr, kJSPropertyAttributeNone},
        {"other", &GetFive, nullptr, kJSPropertyAttributeNone},
        {"dup", &GetTwo, nullptr, kJSPropertyAttributeNone},
        {nullptr, nullptr, nullptr, 0},
    };
    JSStaticFunction functions[] = {
        {"pick", &ReturnOne, kJSPropertyAttributeNone},
        {"pick", &ReturnTwo, kJSPropertyAttributeNone},
        {nullptr, nullptr, 0},
    };
    for (const int version : {0, 1, 1000, -1}) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.version = version;
        definition.className = "Versioned";
        definition.staticValues = values;
        definition.staticFunctions = functions;
        JSClassRef versioned = JSClassCreate(&definition);
        Require(versioned != nullptr);
        JSObjectRef object = JSObjectMake(ctx, versioned, nullptr);
        Require(object != nullptr);
        SetGlobal(ctx, "versioned", object);
        Require(EvalBool(ctx, "versioned.dup === 2 && versioned.other === 5"));
        Require(EvalBool(ctx, "versioned.pick() === 2"));
        Require(EvalBool(ctx, "Object.keys(versioned).filter(function (k) { return k === 'dup'; }).length === 1"));
        JSClassRelease(versioned);
    }

    JSClassDefinition noPrototype = kJSClassDefinitionEmpty;
    noPrototype.attributes = kJSClassAttributeNoAutomaticPrototype;
    noPrototype.staticFunctions = functions;
    JSClassRef noPrototypeClass = JSClassCreate(&noPrototype);
    Require(noPrototypeClass != nullptr);
    SetGlobal(ctx, "flat", JSObjectMake(ctx, noPrototypeClass, nullptr));
    Require(EvalBool(ctx, "flat.pick() === 2"));
    JSClassRelease(noPrototypeClass);

    JSGlobalContextRelease(ctx);
}

void TestToStringTag() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);

    JSClassDefinition named = kJSClassDefinitionEmpty;
    named.className = "Named";
    JSClassRef namedClass = JSClassCreate(&named);
    JSClassDefinition unnamed = kJSClassDefinitionEmpty;
    JSClassRef unnamedClass = JSClassCreate(&unnamed);
    JSClassDefinition derived = kJSClassDefinitionEmpty;
    derived.parentClass = namedClass;
    JSClassRef derivedClass = JSClassCreate(&derived);
    Require(namedClass != nullptr && unnamedClass != nullptr && derivedClass != nullptr);

    SetGlobal(ctx, "named", JSObjectMake(ctx, namedClass, nullptr));
    SetGlobal(ctx, "unnamed", JSObjectMake(ctx, unnamedClass, nullptr));
    SetGlobal(ctx, "derived", JSObjectMake(ctx, derivedClass, nullptr));

    Require(EvalBool(ctx, "Object.prototype.toString.call(named) === '[object Named]'"));
    Require(EvalBool(ctx, "Object.prototype.toString.call(unnamed) === '[object CallbackObject]'"));
    Require(EvalBool(ctx, "Object.prototype.toString.call(derived) === '[object CallbackObject]'"));
    Require(EvalBool(ctx, "var d = Object.getOwnPropertyDescriptor(named, Symbol.toStringTag); d.value === 'Named' && d.writable && !d.enumerable && d.configurable"));

    Require(EvalBool(ctx, "named[Symbol.toStringTag] = 'X'; Object.prototype.toString.call(named) === '[object X]'"));
    Require(EvalBool(ctx, "Object.getOwnPropertyDescriptor(named, Symbol.toStringTag).value === 'X'"));
    Require(EvalBool(ctx, "unnamed[Symbol.toStringTag] = 'Y'; Object.prototype.toString.call(unnamed) === '[object Y]'"));
    Require(EvalBool(ctx, "Object.prototype.toString.call(derived) === '[object CallbackObject]'"));

    JSClassRelease(derivedClass);
    JSClassRelease(unnamedClass);
    JSClassRelease(namedClass);
    JSGlobalContextRelease(ctx);
}

void TestAssignOverPrototypeDataProperty() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    JSClassDefinition shared = kJSClassDefinitionEmpty;
    shared.className = "Shared";
    JSClassRef sharedClass = JSClassCreate(&shared);
    Require(ctx != nullptr && sharedClass != nullptr);

    SetGlobal(ctx, "first", JSObjectMake(ctx, sharedClass, nullptr));
    Require(EvalBool(ctx, "Object.getPrototypeOf(first).value = 1; first.value === 1"));
    for (int i = 0; i < 64; ++i) {
        SetGlobal(ctx, "next", JSObjectMake(ctx, sharedClass, nullptr));
        Require(EvalBool(ctx, "next.value = 2; next.value === 2 && Object.getPrototypeOf(next) === Object.getPrototypeOf(first)"));
    }
    Require(EvalBool(ctx, "first.value === 1 && Object.getPrototypeOf(first).value === 1"));

    JSClassRelease(sharedClass);
    JSGlobalContextRelease(ctx);
}

}

int main() {
    TestStaticFunctionsOnPrototype();
    TestNoAutomaticPrototype();
    TestVersionAndDuplicateNames();
    TestToStringTag();
    TestAssignOverPrototypeDataProperty();
    std::puts("libSceNKWebKit JSC class prototype tests passed");
    return 0;
}
