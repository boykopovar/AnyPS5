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
    std::fprintf(stderr, "JSC constructor check failed at line %u\n", location.line());
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

struct Counters {
    int finalized = 0;
    int hasInstance = 0;
    int hasProperty = 0;
    int setProperty = 0;
    int deleteProperty = 0;
    int propertyNames = 0;
    int called = 0;
};

Counters counters;

void APS5_VABI CountFinalize(JSObjectRef object) {
    (void)object;
    ++counters.finalized;
}

bool APS5_VABI CountHasInstance(JSContextRef ctx, JSObjectRef constructor, JSValueRef possibleInstance, JSValueRef* exception) {
    (void)ctx;
    (void)constructor;
    (void)possibleInstance;
    (void)exception;
    ++counters.hasInstance;
    return false;
}

bool APS5_VABI CountHasProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName) {
    (void)ctx;
    (void)object;
    (void)propertyName;
    ++counters.hasProperty;
    return false;
}

bool APS5_VABI CountSetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception) {
    (void)ctx;
    (void)object;
    (void)propertyName;
    (void)value;
    (void)exception;
    ++counters.setProperty;
    return false;
}

bool APS5_VABI CountDeleteProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception) {
    (void)ctx;
    (void)object;
    (void)propertyName;
    (void)exception;
    ++counters.deleteProperty;
    return false;
}

void APS5_VABI CountPropertyNames(JSContextRef ctx, JSObjectRef object, JSPropertyNameAccumulatorRef propertyNames) {
    (void)ctx;
    (void)object;
    (void)propertyNames;
    ++counters.propertyNames;
}

JSValueRef APS5_VABI CountCall(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)function;
    (void)thisObject;
    (void)argumentCount;
    (void)arguments;
    (void)exception;
    ++counters.called;
    return JSValueMakeNumber(ctx, 7.0);
}

JSValueRef APS5_VABI FixedGetter(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception) {
    (void)object;
    (void)propertyName;
    (void)exception;
    return JSValueMakeNumber(ctx, 5.0);
}

JSObjectRef APS5_VABI NullConstructor(JSContextRef ctx, JSObjectRef constructor, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)ctx;
    (void)constructor;
    (void)argumentCount;
    (void)arguments;
    (void)exception;
    return nullptr;
}

JSObjectRef APS5_VABI ThrowingConstructor(JSContextRef ctx, JSObjectRef constructor, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)constructor;
    (void)argumentCount;
    (void)arguments;
    *exception = JSValueMakeNumber(ctx, 42.0);
    return nullptr;
}

JSObjectRef APS5_VABI PlainConstructor(JSContextRef ctx, JSObjectRef constructor, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)constructor;
    (void)argumentCount;
    (void)arguments;
    (void)exception;
    return JSObjectMake(ctx, nullptr, nullptr);
}

void TestPrototype() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);

    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.className = "Thing";
    JSClassRef thingClass = JSClassCreate(&definition);
    Require(thingClass != nullptr);

    JSObjectRef constructor = JSObjectMakeConstructor(ctx, thingClass, nullptr);
    Require(constructor != nullptr);
    Require(JSObjectIsConstructor(ctx, constructor) && !JSObjectIsFunction(ctx, constructor));
    SetGlobal(ctx, "C", constructor);
    SetGlobal(ctx, "made", JSObjectMake(ctx, thingClass, nullptr));

    Require(EvalBool(ctx, "Object.getPrototypeOf(made) === C.prototype"));
    Require(EvalBool(ctx, "new C() instanceof C"));
    Require(EvalBool(ctx, "made instanceof C"));
    Require(!EvalBool(ctx, "({}) instanceof C"));
    Require(!EvalBool(ctx, "Object.prototype.hasOwnProperty.call(C.prototype, 'constructor')"));
    Require(!EvalBool(ctx, "C.prototype.constructor === C"));
    Require(EvalBool(ctx, "var d = Object.getOwnPropertyDescriptor(C, 'prototype'); !d.writable && !d.enumerable && !d.configurable"));

    JSValueRef exception = nullptr;
    JSObjectRef constructed = JSObjectCallAsConstructor(ctx, constructor, 0, nullptr, &exception);
    Require(constructed != nullptr && exception == nullptr);
    Require(JSValueIsObjectOfClass(ctx, constructed, thingClass));
    SetGlobal(ctx, "constructed", constructed);
    Require(EvalBool(ctx, "constructed instanceof C && Object.getPrototypeOf(constructed) === C.prototype"));

    JSObjectRef classless = JSObjectMakeConstructor(ctx, nullptr, nullptr);
    Require(classless != nullptr);
    SetGlobal(ctx, "N", classless);
    Require(EvalBool(ctx, "N.prototype === Object.prototype"));
    Require(EvalBool(ctx, "new N() instanceof N"));
    Require(EvalBool(ctx, "({}) instanceof N"));
    Require(!EvalBool(ctx, "Object.create(null) instanceof N"));
    Require(!EvalBool(ctx, "N.prototype.constructor === N"));
    Require(EvalBool(ctx, "Object.getPrototypeOf(new N()) === Object.prototype"));

    JSClassDefinition bare = kJSClassDefinitionEmpty;
    bare.attributes = kJSClassAttributeNoAutomaticPrototype;
    JSClassRef bareClass = JSClassCreate(&bare);
    Require(bareClass != nullptr);
    SetGlobal(ctx, "K", JSObjectMakeConstructor(ctx, bareClass, nullptr));
    Require(EvalBool(ctx, "K.prototype === Object.prototype"));
    Require(EvalBool(ctx, "new K() instanceof K"));

    JSClassRelease(bareClass);
    JSClassRelease(thingClass);
    JSGlobalContextRelease(ctx);
}

void TestClassCallbacksStayWithObjects() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);

    JSStaticValue staticValues[] = {
        {"fixed", &FixedGetter, nullptr, kJSPropertyAttributeNone},
        {nullptr, nullptr, nullptr, 0},
    };
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.className = "Probe";
    definition.staticValues = staticValues;
    definition.hasInstance = &CountHasInstance;
    definition.hasProperty = &CountHasProperty;
    definition.setProperty = &CountSetProperty;
    definition.deleteProperty = &CountDeleteProperty;
    definition.getPropertyNames = &CountPropertyNames;
    definition.callAsFunction = &CountCall;
    JSClassRef probeClass = JSClassCreate(&definition);
    Require(probeClass != nullptr);

    JSObjectRef constructor = JSObjectMakeConstructor(ctx, probeClass, nullptr);
    JSObjectRef object = JSObjectMake(ctx, probeClass, nullptr);
    Require(constructor != nullptr && object != nullptr);
    SetGlobal(ctx, "C", constructor);
    SetGlobal(ctx, "probe", object);

    counters = {};
    Require(!EvalBool(ctx, "({}) instanceof C"));
    Require(EvalBool(ctx, "probe instanceof C"));
    Require(counters.hasInstance == 0);
    Require(!EvalBool(ctx, "({}) instanceof probe"));
    Require(counters.hasInstance == 1);

    counters = {};
    Require(!EvalBool(ctx, "'fixed' in C"));
    Require(EvalBool(ctx, "C.fixed === undefined && C.other === undefined"));
    Require(counters.hasProperty == 0);
    Require(EvalBool(ctx, "'fixed' in probe && probe.fixed === 5"));
    Require(counters.hasProperty > 0);

    counters = {};
    Require(EvalBool(ctx, "C.extra = 3; C.extra === 3"));
    Require(counters.setProperty == 0);
    Require(EvalBool(ctx, "probe.extra = 3; probe.extra === 3"));
    Require(counters.setProperty == 1);

    counters = {};
    Require(EvalBool(ctx, "delete C.extra && !('extra' in C)"));
    Require(counters.deleteProperty == 0);
    Require(EvalBool(ctx, "delete probe.extra"));
    Require(counters.deleteProperty == 1);

    counters = {};
    Require(EvalBool(ctx, "Object.keys(C).length === 0"));
    Require(counters.propertyNames == 0);
    Require(EvalBool(ctx, "Object.keys(probe).indexOf('fixed') >= 0"));
    Require(counters.propertyNames > 0);

    counters = {};
    Require(EvalThrows(ctx, "C()"));
    Require(counters.called == 0);
    Require(EvalBool(ctx, "probe() === 7"));
    Require(counters.called == 1);

    int marker = 0;
    CallbackObjectData* constructorData = ctx->group->FindCallbackData(constructor);
    Require(constructorData != nullptr && constructorData->isDirectConstructor);
    Require(!JSObjectSetPrivate(constructor, &marker));
    Require(constructorData->privateData == nullptr);
    constructorData->privateData = &marker;
    Require(JSObjectGetPrivate(constructor) == nullptr);
    constructorData->privateData = nullptr;
    Require(JSObjectSetPrivate(object, &marker));
    Require(JSObjectGetPrivate(object) == &marker);

    Require(!JSValueIsObjectOfClass(ctx, constructor, probeClass));
    Require(JSValueIsObjectOfClass(ctx, object, probeClass));

    ScopedJsString key("secret");
    JSValueRef number = JSValueMakeNumber(ctx, 3.0);
    Require(!JSObjectSetPrivateProperty(ctx, constructor, key, number));
    Require(constructorData->privateProperties.empty());
    PrivatePropertySlot slot;
    slot.handle = const_cast<OpaqueJSValue*>(number);
    slot.strongVal = number->qval;
    constructorData->privateProperties["secret"] = slot;
    Require(JSObjectGetPrivateProperty(ctx, constructor, key) == nullptr);
    Require(!JSObjectDeletePrivateProperty(ctx, constructor, key));
    Require(constructorData->privateProperties.size() == 1);
    Require(JSObjectSetPrivateProperty(ctx, object, key, number));
    Require(JSObjectGetPrivateProperty(ctx, object, key) == number);
    Require(JSObjectDeletePrivateProperty(ctx, object, key));
    Require(JSObjectGetPrivateProperty(ctx, object, key) == nullptr);

    JSClassRelease(probeClass);
    JSGlobalContextRelease(ctx);
}

void TestFinalizeSkipsConstructors() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.finalize = &CountFinalize;
    JSClassRef finalizedClass = JSClassCreate(&definition);
    Require(finalizedClass != nullptr);

    counters = {};
    Require(JSObjectMakeConstructor(ctx, finalizedClass, nullptr) != nullptr);
    Require(JSObjectMake(ctx, finalizedClass, nullptr) != nullptr);
    JSGarbageCollect(ctx);
    JSClassRelease(finalizedClass);
    JSGlobalContextRelease(ctx);
    Require(counters.finalized == 1);
}

void TestConstructorCallbackResult() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);

    JSObjectRef returnsNull = JSObjectMakeConstructor(ctx, nullptr, &NullConstructor);
    SetGlobal(ctx, "ReturnsNull", returnsNull);
    JSValueRef exception = nullptr;
    Require(JSObjectCallAsConstructor(ctx, returnsNull, 0, nullptr, &exception) == nullptr);
    Require(exception != nullptr);
    SetGlobal(ctx, "raised", exception);
    Require(EvalBool(ctx, "raised instanceof TypeError"));
    Require(EvalBool(ctx, "var kind = 'none'; try { new ReturnsNull(); } catch (e) { kind = e instanceof TypeError ? 'type' : 'other'; } kind === 'type'"));

    JSObjectRef throws = JSObjectMakeConstructor(ctx, nullptr, &ThrowingConstructor);
    exception = nullptr;
    Require(JSObjectCallAsConstructor(ctx, throws, 0, nullptr, &exception) == nullptr);
    Require(exception != nullptr && JSValueToNumber(ctx, exception, nullptr) == 42.0);

    JSObjectRef plain = JSObjectMakeConstructor(ctx, nullptr, &PlainConstructor);
    exception = nullptr;
    Require(JSObjectCallAsConstructor(ctx, plain, 0, nullptr, &exception) != nullptr);
    Require(exception == nullptr);

    JSGlobalContextRelease(ctx);
}

}

int main() {
    TestPrototype();
    TestClassCallbacksStayWithObjects();
    TestFinalizeSkipsConstructors();
    TestConstructorCallbackResult();
    std::puts("libSceNKWebKit JSC constructor tests passed");
    return 0;
}
