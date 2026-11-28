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
    std::fprintf(stderr, "JSC put receiver check failed at line %u\n", location.line());
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

bool APS5_VABI CountingSetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception) {
    (void)ctx;
    (void)object;
    (void)value;
    (void)exception;
    ++setPropertyCalls;
    return JSStringIsEqualToUTF8CString(propertyName, "handled");
}

JSValueRef APS5_VABI StaticGetter(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception) {
    (void)object;
    (void)propertyName;
    (void)exception;
    return JSValueMakeNumber(ctx, 3.0);
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

JSValueRef APS5_VABI ReturnSeven(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)function;
    (void)thisObject;
    (void)argumentCount;
    (void)arguments;
    (void)exception;
    return JSValueMakeNumber(ctx, 7.0);
}

const JSStaticValue staticValues[] = {
    {"sv", &StaticGetter, &StaticSetter, kJSPropertyAttributeNone},
    {"ro", &StaticGetter, nullptr, kJSPropertyAttributeReadOnly},
    {nullptr, nullptr, nullptr, 0},
};

const JSStaticFunction staticFunctions[] = {
    {"fn", &ReturnSeven, kJSPropertyAttributeNone},
    {nullptr, nullptr, 0},
};

JSClassRef MakeClass(bool callable) {
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.setProperty = &CountingSetProperty;
    definition.staticValues = staticValues;
    definition.staticFunctions = staticFunctions;
    if (callable) definition.callAsFunction = &ReturnSeven;
    JSClassRef jsClass = JSClassCreate(&definition);
    Require(jsClass != nullptr);
    return jsClass;
}

void ResetCounts() {
    setPropertyCalls = 0;
    staticSetterCalls = 0;
}

bool OwnDataProperty(JSContextRef ctx, const char* object, const char* key, double value) {
    char code[512];
    std::snprintf(code, sizeof(code),
        "(function () { var d = Object.getOwnPropertyDescriptor(%s, %s);"
        " return typeof d === 'object' && d.value === %g && d.writable === true && d.enumerable === true && d.configurable === true; })()",
        object, key, value);
    return EvalBool(ctx, code);
}

void TestTheObjectItselfStillReachesTheClass(JSContextRef ctx) {
    ResetCounts();
    Eval(ctx, "target.handled = 1");
    Require(setPropertyCalls == 1);
    Require(EvalBool(ctx, "Object.getOwnPropertyDescriptor(target, 'handled') === undefined"));
    Eval(ctx, "target.sv = 1");
    Require(staticSetterCalls == 1);
    Require(EvalBool(ctx, "Reflect.set(target, 'handled', 1, target)"));
    Require(setPropertyCalls == 3);
    ScopedJsString key("handled");
    JSValueRef exception = nullptr;
    JSObjectSetProperty(ctx, JSValueToObject(ctx, Eval(ctx, "target"), nullptr), key, JSValueMakeNumber(ctx, 1.0), kJSPropertyAttributeNone, &exception);
    Require(exception == nullptr);
    Require(setPropertyCalls == 4);
}

void TestAnHeirGetsAnOwnProperty(JSContextRef ctx) {
    ResetCounts();
    Eval(ctx, "var heir = Object.create(target); heir.handled = 2; heir.sv = 3; heir.ro = 4; heir.fn = 5; heir.other = 6; heir[0] = 8; heir[Symbol.for('key')] = 9");
    Require(setPropertyCalls == 0);
    Require(staticSetterCalls == 0);
    Require(OwnDataProperty(ctx, "heir", "'handled'", 2.0));
    Require(OwnDataProperty(ctx, "heir", "'sv'", 3.0));
    Require(OwnDataProperty(ctx, "heir", "'ro'", 4.0));
    Require(OwnDataProperty(ctx, "heir", "'fn'", 5.0));
    Require(OwnDataProperty(ctx, "heir", "'other'", 6.0));
    Require(OwnDataProperty(ctx, "heir", "0", 8.0));
    Require(OwnDataProperty(ctx, "heir", "Symbol.for('key')", 9.0));
    Require(EvalBool(ctx, "target.sv === 3 && target.ro === 3 && target.fn() === 7"));
    Require(EvalBool(ctx, "target.other === undefined && target[0] === undefined && target[Symbol.for('key')] === undefined"));
    Require(EvalBool(ctx, "Object.keys(heir).length === 6"));
}

void TestTheCApiOnAnHeir(JSContextRef ctx) {
    ResetCounts();
    JSObjectRef heir = JSValueToObject(ctx, Eval(ctx, "var apiHeir = Object.create(target); apiHeir"), nullptr);
    Require(heir != nullptr);
    ScopedJsString key("handled");
    JSValueRef exception = nullptr;
    JSObjectSetProperty(ctx, heir, key, JSValueMakeNumber(ctx, 12.0), kJSPropertyAttributeNone, &exception);
    Require(exception == nullptr);
    Require(setPropertyCalls == 0);
    Require(OwnDataProperty(ctx, "apiHeir", "'handled'", 12.0));
}

void TestPropertiesStoredOnTheObject(JSContextRef ctx) {
    ResetCounts();
    Eval(ctx, "target.plain = 1;"
              "Object.defineProperty(target, 'frozen', {value: 1, writable: false, configurable: true});"
              "var setterThis = null, setterValue = null;"
              "Object.defineProperty(target, 'accessor', {set: function (v) { setterThis = this; setterValue = v; }, configurable: true});"
              "Object.defineProperty(target, 'getterOnly', {get: function () { return 1; }, configurable: true});"
              "var storeHeir = Object.create(target)");
    ResetCounts();
    Eval(ctx, "storeHeir.plain = 6");
    Require(OwnDataProperty(ctx, "storeHeir", "'plain'", 6.0));
    Require(EvalBool(ctx, "target.plain === 1"));
    Eval(ctx, "storeHeir.frozen = 7");
    Require(EvalBool(ctx, "!Object.prototype.hasOwnProperty.call(storeHeir, 'frozen') && storeHeir.frozen === 1"));
    Require(EvalBool(ctx, "Reflect.set(target, 'frozen', 7, storeHeir) === false"));
    Eval(ctx, "storeHeir.accessor = 8");
    Require(EvalBool(ctx, "setterThis === storeHeir && setterValue === 8"));
    Require(EvalBool(ctx, "!Object.prototype.hasOwnProperty.call(storeHeir, 'accessor')"));
    Eval(ctx, "storeHeir.getterOnly = 9");
    Require(EvalBool(ctx, "!Object.prototype.hasOwnProperty.call(storeHeir, 'getterOnly')"));
    Require(EvalBool(ctx, "Reflect.set(target, 'getterOnly', 9, storeHeir) === false"));
    Require(setPropertyCalls == 0);
    Eval(ctx, "delete target.plain; delete target.frozen; delete target.accessor; delete target.getterOnly");
}

void TestThePrototypeChainOfTheObject(JSContextRef ctx) {
    Eval(ctx, "var chainThis = null, chainValue = null, trapReceiver = null, trapKey = null;"
              "var base = {};"
              "Object.defineProperty(base, 'viaSetter', {set: function (v) { chainThis = this; chainValue = v; }});"
              "Object.defineProperty(base, 'lockedAbove', {value: 1, writable: false});"
              "base.writableAbove = 1;"
              "Object.setPrototypeOf(target, base);"
              "var chainHeir = Object.create(target)");
    ResetCounts();
    Eval(ctx, "chainHeir.viaSetter = 10");
    Require(EvalBool(ctx, "chainThis === chainHeir && chainValue === 10"));
    Require(EvalBool(ctx, "!Object.prototype.hasOwnProperty.call(chainHeir, 'viaSetter')"));
    Eval(ctx, "chainHeir.lockedAbove = 11");
    Require(EvalBool(ctx, "!Object.prototype.hasOwnProperty.call(chainHeir, 'lockedAbove')"));
    Require(EvalBool(ctx, "Reflect.set(target, 'lockedAbove', 11, chainHeir) === false"));
    Eval(ctx, "chainHeir.writableAbove = 12");
    Require(OwnDataProperty(ctx, "chainHeir", "'writableAbove'", 12.0));
    Require(EvalBool(ctx, "base.writableAbove === 1"));

    Eval(ctx, "Object.setPrototypeOf(target, new Proxy({}, {set: function (t, key, value, receiver) { trapKey = key; trapReceiver = receiver; return true; }}));"
              "var proxyHeir = Object.create(target); proxyHeir.throughProxy = 13");
    Require(EvalBool(ctx, "trapKey === 'throughProxy' && trapReceiver === proxyHeir"));
    Require(EvalBool(ctx, "!Object.prototype.hasOwnProperty.call(proxyHeir, 'throughProxy')"));
    Require(setPropertyCalls == 0);
    Eval(ctx, "Object.setPrototypeOf(target, Object.prototype)");

    Require(EvalBool(ctx, "var caught = null; Object.defineProperty(target, 'thrower', {set: function () { throw 'from setter'; }, configurable: true});"
                          "try { Object.create(target).thrower = 1; } catch (error) { caught = error; }"
                          "delete target.thrower; caught === 'from setter'"));
    Require(EvalBool(ctx, "var savedSet = Reflect.set; Reflect.set = function () { throw 'replaced'; };"
                          "var late = Object.create(target); late.handled = 19; Reflect.set = savedSet;"
                          "Object.prototype.hasOwnProperty.call(late, 'handled') && late.handled === 19"));
    Require(setPropertyCalls == 0);
}

void TestOtherReceivers(JSContextRef ctx) {
    ResetCounts();
    Require(EvalBool(ctx, "var other = {}; Reflect.set(target, 'handled', 14, other) === true"));
    Require(OwnDataProperty(ctx, "other", "'handled'", 14.0));
    Require(EvalBool(ctx, "Reflect.set(target, 'handled', 15, 5) === false"));
    Require(EvalBool(ctx, "var sealed = Object.preventExtensions(Object.create(target)); sealed.handled = 16; !Object.prototype.hasOwnProperty.call(sealed, 'handled')"));
    Require(EvalBool(ctx, "Reflect.set(target, 'handled', 16, sealed) === false"));
    Require(EvalBool(ctx, "var locked = Object.defineProperty({}, 'handled', {value: 1, writable: false}); Reflect.set(target, 'handled', 17, locked) === false && locked.handled === 1"));
    Require(EvalBool(ctx, "var second = Object.create(Object.create(target)); second.handled = 18; Object.prototype.hasOwnProperty.call(second, 'handled') && second.handled === 18"));
    Require(setPropertyCalls == 0);
    Require(staticSetterCalls == 0);
}

void TestAnObject(bool callable) {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    JSClassRef jsClass = MakeClass(callable);
    SetGlobal(ctx, "target", JSObjectMake(ctx, jsClass, nullptr));
    TestTheObjectItselfStillReachesTheClass(ctx);
    TestAnHeirGetsAnOwnProperty(ctx);
    TestTheCApiOnAnHeir(ctx);
    TestPropertiesStoredOnTheObject(ctx);
    TestThePrototypeChainOfTheObject(ctx);
    TestOtherReceivers(ctx);
    TestTheObjectItselfStillReachesTheClass(ctx);
    JSClassRelease(jsClass);
    JSGlobalContextRelease(ctx);
}

void TestTheGlobalObject() {
    JSClassRef jsClass = MakeClass(false);
    JSGlobalContextRef ctx = JSGlobalContextCreate(jsClass);
    ResetCounts();
    Eval(ctx, "handled = 1");
    Require(setPropertyCalls == 1);
    Eval(ctx, "this.handled = 1");
    Require(setPropertyCalls == 2);
    Eval(ctx, "sv = 1");
    Require(staticSetterCalls == 1);
    ScopedJsString key("handled");
    JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), key, JSValueMakeNumber(ctx, 1.0), kJSPropertyAttributeNone, nullptr);
    Require(setPropertyCalls == 4);
    Require(EvalBool(ctx, "Reflect.set(this, 'handled', 1, this)"));
    Require(setPropertyCalls == 5);

    ResetCounts();
    Eval(ctx, "var globalHeir = Object.create(this); globalHeir.handled = 20; globalHeir.sv = 21; globalHeir.fresh = 22");
    Require(setPropertyCalls == 0);
    Require(staticSetterCalls == 0);
    Require(OwnDataProperty(ctx, "globalHeir", "'handled'", 20.0));
    Require(OwnDataProperty(ctx, "globalHeir", "'sv'", 21.0));
    Require(OwnDataProperty(ctx, "globalHeir", "'fresh'", 22.0));
    Require(EvalBool(ctx, "this.sv === 3 && !('fresh' in this) && typeof fresh === 'undefined'"));
    Require(EvalBool(ctx, "var declared = 1; globalHeir.declared = 23; declared === 1 && globalHeir.declared === 23"));
    Require(EvalBool(ctx, "var globalOther = {}; Reflect.set(this, 'handled', 24, globalOther) && globalOther.handled === 24"));
    Require(setPropertyCalls == 0);
    JSGlobalContextRelease(ctx);
    JSClassRelease(jsClass);
}

void TestAnObjectOfAReleasedContext() {
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef first = JSGlobalContextCreateInGroup(group, nullptr);
    JSGlobalContextRef second = JSGlobalContextCreateInGroup(group, nullptr);
    JSClassRef jsClass = MakeClass(false);
    SetGlobal(second, "target", JSObjectMake(first, jsClass, nullptr));
    JSGlobalContextRelease(first);
    JSGarbageCollect(second);
    Eval(second, "var filler = []; for (var i = 0; i < 4096; i++) filler.push({ index: i }, function () { return i; });");
    ResetCounts();
    Require(EvalBool(second, "var heir = Object.create(target); heir.handled = 2; heir.other = 3;"
                             "Object.prototype.hasOwnProperty.call(heir, 'handled') && heir.handled === 2 && heir.other === 3"));
    Require(EvalBool(second, "Reflect.set(target, 'handled', 4, filler) && filler.handled === 4"));
    Require(setPropertyCalls == 0);
    JSClassRelease(jsClass);
    JSGlobalContextRelease(second);
    JSContextGroupRelease(group);
}

}  // namespace

int main() {
    TestAnObjectOfAReleasedContext();
    TestAnObject(false);
    TestAnObject(true);
    TestTheGlobalObject();
    std::puts("libSceNKWebKit JS put receiver tests passed");
    return 0;
}
