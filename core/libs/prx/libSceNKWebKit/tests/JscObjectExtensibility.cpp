#include "../src/Jsc.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceJsc/include/Api.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string>

using namespace NKWebKit;

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JSC object extensibility check failed at line %u\n", location.line());
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

int setCalls = 0;
bool acceptEverything = false;

bool APS5_VABI SetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception) {
    (void)ctx;
    (void)object;
    (void)propertyName;
    (void)value;
    (void)exception;
    ++setCalls;
    return acceptEverything;
}

JSValueRef APS5_VABI GetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception) {
    (void)ctx;
    (void)object;
    (void)propertyName;
    (void)exception;
    return nullptr;
}

JSValueRef APS5_VABI StaticValue(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception) {
    (void)object;
    (void)propertyName;
    (void)exception;
    return JSValueMakeNumber(ctx, 41.0);
}

JSValueRef APS5_VABI StaticFunction(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)function;
    (void)thisObject;
    (void)argumentCount;
    (void)arguments;
    (void)exception;
    return JSValueMakeNumber(ctx, 42.0);
}

const char* const kHelpers = "function strictSet(object, key, value) { 'use strict'; try { object[key] = value; return 'stored'; } catch (e) { return e.name; } }"
                             "function defining(object, key, descriptor) { try { Object.defineProperty(object, key, descriptor); return 'defined'; } catch (e) { return e.name; } } 0";

void TestAnObjectThatIsNotExtensible(JSContextRef ctx, const char* name) {
    const std::string bind = std::string("var o = ") + name + "; 0";
    Eval(ctx, bind.c_str());
    Require(EvalBool(ctx, "var key = Symbol('key'); o.kept = 1; o[3] = 'three'; o[key] = 'symbol'; Object.isExtensible(o) && o.kept === 1"));
    Require(EvalBool(ctx, "Object.preventExtensions(o) === o && Object.isExtensible(o) === false"));

    Require(EvalBool(ctx, "o.added = 1; o[4] = 'four'; o[Symbol('late')] = 1; o.added === undefined && !('added' in o) && o[4] === undefined && Reflect.ownKeys(o).length === 3"));
    Require(EvalBool(ctx, "strictSet(o, 'added', 1) === 'TypeError' && strictSet(o, 5, 1) === 'TypeError' && strictSet(o, Symbol('late'), 1) === 'TypeError' && !('added' in o)"));
    Require(EvalBool(ctx, "Reflect.set(o, 'added', 1) === false && Reflect.defineProperty(o, 'added', { value: 1 }) === false && !('added' in o)"));
    Require(EvalBool(ctx, "defining(o, 'added', { value: 1 }) === 'TypeError' && defining(o, 6, { value: 1 }) === 'TypeError' && defining(o, 'added', { get: function () { return 1; } }) === 'TypeError'"));
    Require(EvalBool(ctx, "var failed = false; try { Object.assign(o, { added: 1 }); } catch (e) { failed = e instanceof TypeError; } failed && !('added' in o)"));

    Require(EvalBool(ctx, "o.kept = 2; o[3] = 'trois'; o[key] = 'changed'; o.kept === 2 && o[3] === 'trois' && o[key] === 'changed' && strictSet(o, 'kept', 3) === 'stored' && o.kept === 3"));
    Require(EvalBool(ctx, "defining(o, 'kept', { value: 4, enumerable: false }) === 'defined' && o.kept === 4 && Object.keys(o).indexOf('kept') < 0"));
    Require(EvalBool(ctx, "defining(o, 'kept', { get: function () { return 'got'; }, configurable: true }) === 'defined' && o.kept === 'got'"));
    Require(EvalBool(ctx, "delete o[3] && o[3] === undefined && (o[3] = 'again', o[3] === undefined) && strictSet(o, 3, 'again') === 'TypeError'"));

    Require(EvalBool(ctx, "Object.seal(o) === o && Object.isSealed(o) && delete o.kept === false && o.kept === 'got' && strictSet(o, 'other', 1) === 'TypeError'"));
    Require(EvalBool(ctx, "o[key] = 'still writable'; o[key] === 'still writable'"));
    Require(EvalBool(ctx, "Object.freeze(o) === o && Object.isFrozen(o) && (o[key] = 'no', o[key] === 'still writable') && strictSet(o, key, 'no') === 'TypeError' && strictSet(o, 'other', 1) === 'TypeError'"));
    Require(EvalBool(ctx, "defining(o, 'other', { value: 1 }) === 'TypeError' && Reflect.ownKeys(o).length === 2"));
}

void TestTheCallbacksAreStillAsked(JSContextRef ctx) {
    Eval(ctx, "var o = asked; Object.preventExtensions(o); 0");
    setCalls = 0;
    Require(EvalBool(ctx, "o.declined = 1; o.declined === undefined"));
    Require(setCalls == 1);
    acceptEverything = true;
    Require(EvalBool(ctx, "strictSet(o, 'accepted', 1) === 'stored' && Reflect.set(o, 'accepted', 2) === true"));
    Require(setCalls == 3);
    acceptEverything = false;
}

void TestWhatTheClassSupplies(JSContextRef ctx) {
    Require(EvalBool(ctx, "var s = supplied; s.own = 1; s.fixed === 41 && s.call() === 42"));
    Require(EvalBool(ctx, "var raised = null; try { Object.freeze(s); } catch (e) { raised = e; } raised === null && Object.isExtensible(s) === false"));
    Require(EvalBool(ctx, "s.fixed === 41 && s.call() === 42 && s.own === 1 && (s.own = 2, s.own === 1) && strictSet(s, 'more', 1) === 'TypeError'"));

    Require(EvalBool(ctx, "var t = suppliedToo; Object.preventExtensions(t); t.call = 'override'; t.call === 'override'"));
    Require(EvalBool(ctx, "defining(t, 'fixed', { value: 5, configurable: true }) === 'defined' && defining(t, 'missing', { value: 5 }) === 'TypeError'"));
}

void TestRefusalsFromThePrototypeChain(JSContextRef ctx) {
    Eval(ctx, "var parent = {}; Object.defineProperty(parent, 'fixed', { value: 1, writable: false }); Object.defineProperty(parent, 'computed', { get: function () { return 2; } });"
              "var stored = []; Object.defineProperty(parent, 'accessor', { get: function () { return 3; }, set: function (v) { stored.push(v); } }); var c = chained; Object.setPrototypeOf(c, parent); 0");
    Require(EvalBool(ctx, "c.fixed = 9; c.computed = 9; c.fixed === 1 && c.computed === 2 && !c.hasOwnProperty('fixed') && !c.hasOwnProperty('computed')"));
    Require(EvalBool(ctx, "strictSet(c, 'fixed', 9) === 'TypeError' && strictSet(c, 'computed', 9) === 'TypeError' && Reflect.set(c, 'fixed', 9) === false && Reflect.set(c, 'computed', 9) === false"));
    Require(EvalBool(ctx, "c.accessor = 'a'; strictSet(c, 'accessor', 'b') === 'stored' && stored.join() === 'a,b' && !c.hasOwnProperty('accessor')"));
    Require(EvalBool(ctx, "strictSet(c, 'fresh', 1) === 'stored' && c.fresh === 1 && c.hasOwnProperty('fresh')"));
}

}  // namespace

int main() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    Eval(ctx, kHelpers);

    JSClassDefinition plainDefinition = kJSClassDefinitionEmpty;
    plainDefinition.className = "Plain";
    JSClassRef plainClass = JSClassCreate(&plainDefinition);

    JSClassDefinition busyDefinition = kJSClassDefinitionEmpty;
    busyDefinition.className = "Busy";
    busyDefinition.setProperty = &SetProperty;
    busyDefinition.getProperty = &GetProperty;
    JSClassRef busyClass = JSClassCreate(&busyDefinition);

    const JSStaticValue staticValues[] = {
        {"fixed", &StaticValue, nullptr, kJSPropertyAttributeNone},
        {nullptr, nullptr, nullptr, 0},
    };
    const JSStaticFunction staticFunctions[] = {
        {"call", &StaticFunction, kJSPropertyAttributeNone},
        {nullptr, nullptr, 0},
    };
    JSClassDefinition suppliedDefinition = kJSClassDefinitionEmpty;
    suppliedDefinition.className = "Supplied";
    suppliedDefinition.attributes = kJSClassAttributeNoAutomaticPrototype;
    suppliedDefinition.staticValues = staticValues;
    suppliedDefinition.staticFunctions = staticFunctions;
    JSClassRef suppliedClass = JSClassCreate(&suppliedDefinition);

    SetGlobal(ctx, "plain", JSObjectMake(ctx, plainClass, nullptr));
    SetGlobal(ctx, "busy", JSObjectMake(ctx, busyClass, nullptr));
    SetGlobal(ctx, "asked", JSObjectMake(ctx, busyClass, nullptr));
    SetGlobal(ctx, "supplied", JSObjectMake(ctx, suppliedClass, nullptr));
    SetGlobal(ctx, "suppliedToo", JSObjectMake(ctx, suppliedClass, nullptr));
    SetGlobal(ctx, "chained", JSObjectMake(ctx, plainClass, nullptr));
    SetGlobal(ctx, "classless", JSObjectMake(ctx, nullptr, nullptr));

    TestAnObjectThatIsNotExtensible(ctx, "plain");
    TestAnObjectThatIsNotExtensible(ctx, "busy");
    TestAnObjectThatIsNotExtensible(ctx, "classless");
    TestTheCallbacksAreStillAsked(ctx);
    TestWhatTheClassSupplies(ctx);
    TestRefusalsFromThePrototypeChain(ctx);

    JSClassRelease(suppliedClass);
    JSClassRelease(busyClass);
    JSClassRelease(plainClass);
    JSGlobalContextRelease(ctx);
    std::puts("libSceNKWebKit JS object extensibility tests passed");
    return 0;
}
