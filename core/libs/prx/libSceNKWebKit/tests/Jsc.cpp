#include "../src/Jsc.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceJsc/include/Api.hpp"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace NKWebKit;

namespace {

void Require(bool condition) {
    if (!condition) std::abort();
}

struct ScopedJsString {
    JSStringRef ref = nullptr;
    explicit ScopedJsString(const char* utf8) : ref(JSStringCreateWithUTF8CString_nid_postfix(utf8)) {}
    explicit ScopedJsString(JSStringRef r) : ref(r) {}
    ~ScopedJsString() {
        if (ref) JSStringRelease_nid_postfix(ref);
    }
    operator JSStringRef() const { return ref; }
};

JSValueRef EvalUtf8(JSContextRef ctx, const char* code, JSValueRef* exception = nullptr) {
    ScopedJsString script(code);
    return JSEvaluateScript(ctx, script, nullptr, nullptr, 1, exception);
}

struct ClassTestState {
    std::vector<int> initOrder;
    std::vector<int> finalizeOrder;
    double staticVal = 10.0;
    double dynamicVal = 77.0;
    bool dynamicDeleted = false;
    bool throwOnGet = false;
    bool throwOnSet = false;
};

void APS5_VABI BaseInitCallback(JSContextRef ctx, JSObjectRef object) {
    (void)ctx;
    auto* state = static_cast<ClassTestState*>(JSObjectGetPrivate(object));
    if (state) state->initOrder.push_back(1);
}

void APS5_VABI BaseFinalizeCallback(JSObjectRef object) {
    auto* state = static_cast<ClassTestState*>(JSObjectGetPrivate(object));
    if (state) state->finalizeOrder.push_back(1);
}

void APS5_VABI DerivedInitCallback(JSContextRef ctx, JSObjectRef object) {
    (void)ctx;
    auto* state = static_cast<ClassTestState*>(JSObjectGetPrivate(object));
    if (state) state->initOrder.push_back(2);
}

void APS5_VABI DerivedFinalizeCallback(JSObjectRef object) {
    auto* state = static_cast<ClassTestState*>(JSObjectGetPrivate(object));
    if (state) state->finalizeOrder.push_back(2);
}

JSValueRef APS5_VABI StaticValueGetter(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception) {
    (void)propertyName;
    (void)exception;
    auto* state = static_cast<ClassTestState*>(JSObjectGetPrivate(object));
    return JSValueMakeNumber(ctx, state ? state->staticVal : 0.0);
}

bool APS5_VABI StaticValueSetter(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception) {
    (void)propertyName;
    auto* state = static_cast<ClassTestState*>(JSObjectGetPrivate(object));
    if (!state) return false;
    state->staticVal = JSValueToNumber(ctx, value, exception);
    return true;
}

JSValueRef APS5_VABI StaticReadOnlyGetter(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception) {
    (void)object;
    (void)propertyName;
    (void)exception;
    return JSValueMakeNumber(ctx, 999.0);
}

JSValueRef APS5_VABI StaticMultiplyMethod(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)function;
    auto* state = static_cast<ClassTestState*>(JSObjectGetPrivate(thisObject));
    const double factor = argumentCount > 0 ? JSValueToNumber(ctx, arguments[0], exception) : 1.0;
    return JSValueMakeNumber(ctx, (state ? state->staticVal : 0.0) * factor);
}

bool APS5_VABI DynamicHasProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName) {
    (void)ctx;
    auto* state = static_cast<ClassTestState*>(JSObjectGetPrivate(object));
    if (!state || state->dynamicDeleted) return false;
    return JSStringIsEqualToUTF8CString(propertyName, "dynProp") || JSStringIsEqualToUTF8CString(propertyName, "throwProp");
}

JSValueRef APS5_VABI DynamicGetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception) {
    auto* state = static_cast<ClassTestState*>(JSObjectGetPrivate(object));
    if (!state || state->dynamicDeleted) return nullptr;
    if (JSStringIsEqualToUTF8CString(propertyName, "throwProp") || state->throwOnGet) {
        ScopedJsString msg("getProperty error");
        JSValueRef arg = EvalUtf8(ctx, "'getProperty error'");
        *exception = JSObjectMakeError(ctx, 1, &arg, nullptr);
        return nullptr;
    }
    if (JSStringIsEqualToUTF8CString(propertyName, "dynProp")) {
        return JSValueMakeNumber(ctx, state->dynamicVal);
    }
    return nullptr;
}

bool APS5_VABI DynamicSetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception) {
    auto* state = static_cast<ClassTestState*>(JSObjectGetPrivate(object));
    if (!state) return false;
    if (state->throwOnSet) {
        JSValueRef arg = EvalUtf8(ctx, "'setProperty error'");
        *exception = JSObjectMakeError(ctx, 1, &arg, nullptr);
        return false;
    }
    if (JSStringIsEqualToUTF8CString(propertyName, "dynProp")) {
        state->dynamicVal = JSValueToNumber(ctx, value, exception);
        state->dynamicDeleted = false;
        return true;
    }
    return false;
}

bool APS5_VABI DynamicDeleteProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception) {
    (void)ctx;
    (void)exception;
    auto* state = static_cast<ClassTestState*>(JSObjectGetPrivate(object));
    if (!state) return false;
    if (JSStringIsEqualToUTF8CString(propertyName, "dynProp")) {
        state->dynamicDeleted = true;
        return true;
    }
    return false;
}

void APS5_VABI DynamicGetPropertyNames(JSContextRef ctx, JSObjectRef object, JSPropertyNameAccumulatorRef propertyNames) {
    (void)ctx;
    auto* state = static_cast<ClassTestState*>(JSObjectGetPrivate(object));
    if (state && !state->dynamicDeleted) {
        ScopedJsString dynName("dynProp");
        JSPropertyNameAccumulatorAddName(propertyNames, dynName);
    }
    ScopedJsString extraName("enumeratedExtra");
    JSPropertyNameAccumulatorAddName(propertyNames, extraName);
}

JSValueRef APS5_VABI ClassCallAsFunction(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)thisObject;
    auto* state = static_cast<ClassTestState*>(JSObjectGetPrivate(function));
    double sum = state ? state->staticVal : 0.0;
    for (std::size_t i = 0; i < argumentCount; ++i) {
        sum += JSValueToNumber(ctx, arguments[i], exception);
    }
    return JSValueMakeNumber(ctx, sum);
}

JSObjectRef APS5_VABI ClassCallAsConstructor(JSContextRef ctx, JSObjectRef constructor, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)constructor;
    JSObjectRef obj = JSObjectMake(ctx, nullptr, nullptr);
    const double v = argumentCount > 0 ? JSValueToNumber(ctx, arguments[0], exception) : 0.0;
    ScopedJsString prop("constructedVal");
    JSObjectSetProperty(ctx, obj, prop, JSValueMakeNumber(ctx, v), kJSPropertyAttributeNone, exception);
    return obj;
}

bool APS5_VABI ClassHasInstance(JSContextRef ctx, JSObjectRef constructor, JSValueRef possibleInstance, JSValueRef* exception) {
    (void)constructor;
    if (!JSValueIsObject(ctx, possibleInstance)) return false;
    JSObjectRef obj = JSValueToObject(ctx, possibleInstance, exception);
    ScopedJsString marker("customInstanceMarker");
    return JSObjectHasProperty(ctx, obj, marker);
}

JSValueRef APS5_VABI ClassConvertToType(JSContextRef ctx, JSObjectRef object, JSType type, JSValueRef* exception) {
    (void)exception;
    auto* state = static_cast<ClassTestState*>(JSObjectGetPrivate(object));
    if (type == kJSTypeString) {
        return EvalUtf8(ctx, "'converted-string'");
    }
    return JSValueMakeNumber(ctx, state ? state->staticVal + 5.0 : 5.0);
}

JSValueRef APS5_VABI StandaloneCallbackFunction(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    Require(function != nullptr);
    Require(thisObject != nullptr);
    if (argumentCount == 1 && JSValueIsBoolean(ctx, arguments[0]) && !JSValueToBoolean(ctx, arguments[0])) {
        JSValueRef msg = EvalUtf8(ctx, "'callback failure'");
        *exception = JSObjectMakeError(ctx, 1, &msg, nullptr);
        return nullptr;
    }
    double prod = 1.0;
    for (std::size_t i = 0; i < argumentCount; ++i) {
        prod *= JSValueToNumber(ctx, arguments[i], exception);
    }
    return JSValueMakeNumber(ctx, prod);
}

void APS5_VABI NoCopyDeallocator(void* bytes, void* deallocatorContext) {
    auto* count = static_cast<int*>(deallocatorContext);
    if (count) ++(*count);
    std::free(bytes);
}

void APS5_VABI HeapFinalizerCallback(JSContextGroupRef group, void* userData) {
    (void)group;
    auto* counter = static_cast<int*>(userData);
    if (counter) ++(*counter);
}

struct ConstraintState {
    JSObjectRef parentObj = nullptr;
    JSObjectRef childObj = nullptr;
};

void APS5_VABI MarkingConstraintCallback(JSMarker* marker, void* userData) {
    auto* st = static_cast<ConstraintState*>(userData);
    if (!st || !marker) return;
    if (st->parentObj && marker->IsMarked(marker, st->parentObj) && st->childObj) {
        marker->Mark(marker, st->childObj);
    }
}

void APS5_VABI WeakMapDestroyed(OpaqueJSWeakObjectMap* map, void* data) {
    (void)map;
    auto* flag = static_cast<bool*>(data);
    if (flag) *flag = true;
}

bool APS5_VABI ShouldTerminateWatchdog(JSContextRef ctx, void* context) {
    Require(ctx != nullptr);
    auto* calls = static_cast<int*>(context);
    if (!calls) return true;
    ++(*calls);
    return *calls >= 2;
}

JSValueRef APS5_VABI UnhandledRejectionHandler(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)function;
    (void)thisObject;
    (void)exception;
    if (argumentCount >= 2) {
        ScopedJsString key("lastUnhandledReason");
        JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), key, arguments[1], kJSPropertyAttributeNone, nullptr);
    }
    return JSValueMakeUndefined(ctx);
}

void TestStringsAndOpaqueDestructors() {
    const std::uint16_t units[] = {'H', 'i', '!'};
    GuestJSString* str = JSStringCreateWithCharactersNoCopy(units, 3);
    Require(str != nullptr);
    Require(JSStringGetLength_nid_postfix(str) == 3);
    Require(JSStringIsEqualToUTF8CString(str, "Hi!"));
    JSStringRelease_nid_postfix(str);

    GuestJSString* empty = JSStringCreateWithCharactersNoCopy(nullptr, 0);
    Require(empty != nullptr);
    Require(JSStringGetLength_nid_postfix(empty) == 0);
    JSStringRelease_nid_postfix(empty);

    alignas(GuestJSString) unsigned char storage1[sizeof(GuestJSString)];
    auto* s1 = new (storage1) GuestJSString();
    s1->characters = {'a', 0};
    _ZN14OpaqueJSStringD1Ev(s1);

    alignas(GuestJSString) unsigned char storage2[sizeof(GuestJSString)];
    auto* s2 = new (storage2) GuestJSString();
    s2->characters = {'b', 0};
    _ZN14OpaqueJSStringD2Ev(s2);

    JSClassDefinition def = kJSClassDefinitionEmpty;
    def.className = "InPlaceClass";
    JSClassRef c1 = JSClassCreate(&def);
    _ZN13OpaqueJSClassD1Ev(c1);
    operator delete(c1);

    JSClassRef c2 = JSClassCreate(&def);
    _ZN13OpaqueJSClassD2Ev(c2);
    operator delete(c2);
}

void TestContextsAndScripts() {
    Require(JSRemoteInspectorGetInspectionEnabledByDefault());
    JSRemoteInspectorSetInspectionEnabledByDefault(false);
    Require(!JSRemoteInspectorGetInspectionEnabledByDefault());

    JSContextGroupRef group = JSContextGroupCreate();
    Require(group != nullptr);
    Require(JSContextGroupRetain(group) == group);
    JSContextGroupRelease(group);

    JSGlobalContextRef ctx = JSGlobalContextCreateInGroup(group, nullptr);
    Require(ctx != nullptr);
    Require(JSContextGetGroup(ctx) == group);
    Require(JSContextGetGlobalContext(ctx) == ctx);
    Require(!JSGlobalContextGetRemoteInspectionEnabled(ctx));
    JSGlobalContextSetRemoteInspectionEnabled(ctx, true);
    Require(JSGlobalContextGetRemoteInspectionEnabled(ctx));
    JSRemoteInspectorSetInspectionEnabledByDefault(true);

    Require(JSGlobalContextGetIncludesNativeCallStackWhenReportingExceptions(ctx));
    JSGlobalContextSetIncludesNativeCallStackWhenReportingExceptions(ctx, false);
    Require(!JSGlobalContextGetIncludesNativeCallStackWhenReportingExceptions(ctx));

    Require(JSGlobalContextCopyName(ctx) == nullptr);
    ScopedJsString ctxName("MainPS5Context");
    JSGlobalContextSetName(ctx, ctxName);
    JSStringRef copiedName = JSGlobalContextCopyName(ctx);
    Require(copiedName != nullptr);
    Require(JSStringIsEqualToUTF8CString(copiedName, "MainPS5Context"));
    JSStringRelease_nid_postfix(copiedName);
    JSGlobalContextSetName(ctx, nullptr);
    Require(JSGlobalContextCopyName(ctx) == nullptr);

    ScopedJsString validSyntax("let a = 40 + 2;");
    JSValueRef exc = nullptr;
    Require(JSCheckScriptSyntax(ctx, validSyntax, nullptr, 1, &exc));
    Require(exc == nullptr);

    ScopedJsString badSyntax("function ( {");
    Require(!JSCheckScriptSyntax(ctx, badSyntax, nullptr, 1, &exc));
    Require(exc != nullptr);

    const char* asciiSrc = "this.customMul * 3";
    JSStringRef errMsg = nullptr;
    int errLine = 0;
    ScopedJsString scriptUrl("test://ascii.js");
    JSScriptRef script = JSScriptCreateReferencingImmortalASCIIText(group, scriptUrl, 10, asciiSrc, std::strlen(asciiSrc), &errMsg, &errLine);
    Require(script != nullptr);
    Require(errMsg == nullptr);
    JSScriptRetain(script);
    JSScriptRelease(script);

    const char nonAsciiSrc[] = {'1', '+', '\x80', '\0'};
    Require(JSScriptCreateReferencingImmortalASCIIText(group, scriptUrl, 1, nonAsciiSrc, 3, nullptr, nullptr) == nullptr);

    const char* badScriptSrc = "\n\nlet x = ;";
    JSScriptRef badScript = JSScriptCreateReferencingImmortalASCIIText(group, scriptUrl, 10, badScriptSrc, std::strlen(badScriptSrc), &errMsg, &errLine);
    Require(badScript == nullptr);
    Require(errMsg != nullptr);
    Require(errLine == 12);
    JSStringRelease_nid_postfix(errMsg);

    JSObjectRef customThis = JSObjectMake(ctx, nullptr, nullptr);
    ScopedJsString mulKey("customMul");
    JSObjectSetProperty(ctx, customThis, mulKey, JSValueMakeNumber(ctx, 14.0), kJSPropertyAttributeNone, nullptr);
    exc = nullptr;
    JSValueRef evalRes = JSScriptEvaluate(ctx, script, customThis, &exc);
    Require(exc == nullptr);
    Require(evalRes != nullptr);
    Require(JSValueToNumber(ctx, evalRes, nullptr) == 42.0);

    ScopedJsString fromStrSrc("21 * 2");
    JSScriptRef strScript = JSScriptCreateFromString(group, scriptUrl, 1, fromStrSrc, nullptr, nullptr);
    Require(strScript != nullptr);
    JSValueRef strEvalRes = JSScriptEvaluate(ctx, strScript, nullptr, &exc);
    Require(JSValueToNumber(ctx, strEvalRes, nullptr) == 42.0);

    JSGlobalContextRef otherGroupCtx = JSGlobalContextCreate(nullptr);
    Require(JSScriptEvaluate(otherGroupCtx, strScript, nullptr, nullptr) == nullptr);
    JSGlobalContextRelease(otherGroupCtx);

    JSScriptRelease(strScript);
    JSScriptRelease(script);
    JSGlobalContextRetain(ctx);
    JSGlobalContextRelease(ctx);
    JSGlobalContextRelease(ctx);
    JSContextGroupRelease(group);
}

void TestValuesAndObjects() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    JSValueRef undefVal = JSValueMakeUndefined(ctx);
    JSValueRef nullVal = JSValueMakeNull(ctx);
    JSValueRef trueVal = JSValueMakeBoolean(ctx, true);
    JSValueRef falseVal = JSValueMakeBoolean(ctx, false);
    JSValueRef numVal = JSValueMakeNumber(ctx, 123.5);
    ScopedJsString symDesc("mySym");
    JSValueRef symVal = JSValueMakeSymbol(ctx, symDesc);
    JSValueRef strVal = EvalUtf8(ctx, "'hello world'");

    Require(JSValueGetType(ctx, undefVal) == kJSTypeUndefined && JSValueIsUndefined(ctx, undefVal));
    Require(JSValueGetType(ctx, nullVal) == kJSTypeNull && JSValueIsNull(ctx, nullVal));
    Require(JSValueGetType(ctx, trueVal) == kJSTypeBoolean && JSValueIsBoolean(ctx, trueVal) && JSValueToBoolean(ctx, trueVal));
    Require(JSValueGetType(ctx, falseVal) == kJSTypeBoolean && !JSValueToBoolean(ctx, falseVal));
    Require(JSValueGetType(ctx, numVal) == kJSTypeNumber && JSValueIsNumber(ctx, numVal) && JSValueToNumber(ctx, numVal, nullptr) == 123.5);
    Require(JSValueGetType(ctx, symVal) == kJSTypeSymbol && JSValueIsSymbol(ctx, symVal));
    Require(JSValueGetType(ctx, strVal) == kJSTypeString && JSValueIsString(ctx, strVal));

    JSValueRef looseNum = EvalUtf8(ctx, "'123.5'");
    Require(JSValueIsEqual(ctx, numVal, looseNum, nullptr));
    Require(!JSValueIsStrictEqual(ctx, numVal, looseNum));
    Require(JSValueIsStrictEqual(ctx, numVal, JSValueMakeNumber(ctx, 123.5)));

    ScopedJsString jsonInput("{\"a\":10,\"b\":[true,\"ok\"]}");
    JSValueRef parsedJson = JSValueMakeFromJSONString(ctx, jsonInput);
    Require(parsedJson != nullptr && JSValueIsObject(ctx, parsedJson));
    ScopedJsString badJson("{bad");
    Require(JSValueMakeFromJSONString(ctx, badJson) == nullptr);
    JSStringRef jsonOut = JSValueCreateJSONString(ctx, parsedJson, 0, nullptr);
    Require(jsonOut != nullptr);
    Require(JSStringIsEqualToUTF8CString(jsonOut, "{\"a\":10,\"b\":[true,\"ok\"]}"));
    JSStringRelease_nid_postfix(jsonOut);
    Require(JSValueCreateJSONString(ctx, undefVal, 0, nullptr) == nullptr);

    JSValueRef arrItems[2] = {JSValueMakeNumber(ctx, 10.0), JSValueMakeNumber(ctx, 20.0)};
    JSObjectRef arr = JSObjectMakeArray(ctx, 2, arrItems, nullptr);
    Require(JSValueIsArray(ctx, arr));
    Require(JSValueToNumber(ctx, JSObjectGetPropertyAtIndex(ctx, arr, 1, nullptr), nullptr) == 20.0);
    JSObjectSetPropertyAtIndex(ctx, arr, 2, JSValueMakeNumber(ctx, 30.0), nullptr);
    Require(JSValueToNumber(ctx, JSObjectGetPropertyAtIndex(ctx, arr, 2, nullptr), nullptr) == 30.0);

    JSValueRef dateArg = JSValueMakeNumber(ctx, 1700000000000.0);
    JSObjectRef dateObj = JSObjectMakeDate(ctx, 1, &dateArg, nullptr);
    Require(JSValueIsDate(ctx, dateObj));

    JSValueRef errArg = EvalUtf8(ctx, "'boom'");
    JSObjectRef errObj = JSObjectMakeError(ctx, 1, &errArg, nullptr);
    ScopedJsString msgProp("message");
    JSStringRef errMsgStr = JSValueToStringCopy(ctx, JSObjectGetProperty(ctx, errObj, msgProp, nullptr), nullptr);
    Require(JSStringIsEqualToUTF8CString(errMsgStr, "boom"));
    JSStringRelease_nid_postfix(errMsgStr);

    JSValueRef rxArg = EvalUtf8(ctx, "'abc+'");
    JSObjectRef rxObj = JSObjectMakeRegExp(ctx, 1, &rxArg, nullptr);
    Require(rxObj != nullptr);

    ScopedJsString fnName("addTwo");
    ScopedJsString p1("x");
    ScopedJsString p2("y");
    JSStringRef params[2] = {p1, p2};
    ScopedJsString fnBody("return x + y;");
    JSObjectRef jsFn = JSObjectMakeFunction(ctx, fnName, 2, params, fnBody, nullptr, 1, nullptr);
    Require(jsFn != nullptr && JSObjectIsFunction(ctx, jsFn));
    JSValueRef callArgs[2] = {JSValueMakeNumber(ctx, 15.0), JSValueMakeNumber(ctx, 27.0)};
    JSValueRef callRes = JSObjectCallAsFunction(ctx, jsFn, nullptr, 2, callArgs, nullptr);
    Require(JSValueToNumber(ctx, callRes, nullptr) == 42.0);

    ScopedJsString cbName("mulCb");
    JSObjectRef cbFn = JSObjectMakeFunctionWithCallback(ctx, cbName, &StandaloneCallbackFunction);
    Require(JSObjectIsFunction(ctx, cbFn) && !JSObjectIsConstructor(ctx, cbFn));
    JSValueRef cbRes = JSObjectCallAsFunction(ctx, cbFn, nullptr, 2, callArgs, nullptr);
    Require(JSValueToNumber(ctx, cbRes, nullptr) == 405.0);
    JSValueRef failArg = JSValueMakeBoolean(ctx, false);
    JSValueRef cbExc = nullptr;
    Require(JSObjectCallAsFunction(ctx, cbFn, nullptr, 1, &failArg, &cbExc) == nullptr);
    Require(cbExc != nullptr);

    JSObjectRef propObj = JSObjectMake(ctx, nullptr, nullptr);
    ScopedJsString roKey("readOnlyKey");
    JSObjectSetProperty(ctx, propObj, roKey, JSValueMakeNumber(ctx, 55.0), kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, nullptr);
    JSObjectSetProperty(ctx, propObj, roKey, JSValueMakeNumber(ctx, 88.0), kJSPropertyAttributeNone, nullptr);
    Require(JSValueToNumber(ctx, JSObjectGetProperty(ctx, propObj, roKey, nullptr), nullptr) == 55.0);
    Require(!JSObjectDeleteProperty(ctx, propObj, roKey, nullptr));

    JSObjectSetPropertyForKey(ctx, propObj, symVal, JSValueMakeNumber(ctx, 777.0), kJSPropertyAttributeNone, nullptr);
    Require(JSValueToNumber(ctx, JSObjectGetPropertyForKey(ctx, propObj, symVal, nullptr), nullptr) == 777.0);
    Require(JSObjectDeletePropertyForKey(ctx, propObj, symVal, nullptr));
    Require(JSValueIsUndefined(ctx, JSObjectGetPropertyForKey(ctx, propObj, symVal, nullptr)));

    JSObjectRef protoObj = JSObjectMake(ctx, nullptr, nullptr);
    ScopedJsString inheritedKey("inheritedVal");
    JSObjectSetProperty(ctx, protoObj, inheritedKey, JSValueMakeNumber(ctx, 321.0), kJSPropertyAttributeNone, nullptr);
    JSObjectSetPrototype(ctx, propObj, protoObj);
    Require(JSValueIsStrictEqual(ctx, JSObjectGetPrototype(ctx, propObj), protoObj));
    Require(JSObjectHasProperty(ctx, propObj, inheritedKey));
    Require(JSValueToNumber(ctx, JSObjectGetProperty(ctx, propObj, inheritedKey, nullptr), nullptr) == 321.0);

    JSObjectRef proxyObj = JSValueToObject(ctx, EvalUtf8(ctx, "new Proxy({k: 9}, {})"), nullptr);
    JSObjectRef proxyTarget = JSObjectGetProxyTarget(proxyObj);
    Require(proxyTarget != nullptr);
    ScopedJsString kProp("k");
    Require(JSValueToNumber(ctx, JSObjectGetProperty(ctx, proxyTarget, kProp, nullptr), nullptr) == 9.0);
    Require(JSObjectGetGlobalContext(proxyTarget) == ctx);

    JSObjectRef resolveFn = nullptr;
    JSObjectRef rejectFn = nullptr;
    JSObjectRef promise = JSObjectMakeDeferredPromise(ctx, &resolveFn, &rejectFn, nullptr);
    Require(promise != nullptr && resolveFn != nullptr && rejectFn != nullptr);
    ScopedJsString promProp("testPromise");
    JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), promProp, promise, kJSPropertyAttributeNone, nullptr);
    EvalUtf8(ctx, "var resolvedOut = 0; testPromise.then(v => { resolvedOut = v + 1; });");
    JSValueRef resolveVal = JSValueMakeNumber(ctx, 41.0);
    JSObjectCallAsFunction(ctx, resolveFn, nullptr, 1, &resolveVal, nullptr);
    Require(JSValueToNumber(ctx, EvalUtf8(ctx, "resolvedOut"), nullptr) == 42.0);

    JSObjectRef rejHandler = JSObjectMakeFunctionWithCallback(ctx, nullptr, &UnhandledRejectionHandler);
    JSGlobalContextSetUnhandledRejectionCallback(ctx, rejHandler, nullptr);
    JSObjectRef rejResolve = nullptr;
    JSObjectRef rejReject = nullptr;
    JSObjectMakeDeferredPromise(ctx, &rejResolve, &rejReject, nullptr);
    JSValueRef reasonVal = JSValueMakeNumber(ctx, 503.0);
    JSObjectCallAsFunction(ctx, rejReject, nullptr, 1, &reasonVal, nullptr);
    Require(JSValueToNumber(ctx, EvalUtf8(ctx, "lastUnhandledReason"), nullptr) == 503.0);

    JSGlobalContextRelease(ctx);
}

void TestClassesAndCallbacks() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);

    JSStaticValue baseStaticValues[] = {
        {"staticVal", &StaticValueGetter, &StaticValueSetter, kJSPropertyAttributeDontDelete},
        {"readOnlyStatic", &StaticReadOnlyGetter, nullptr, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum},
        {nullptr, nullptr, nullptr, 0},
    };
    JSStaticFunction baseStaticFunctions[] = {
        {"multiply", &StaticMultiplyMethod, kJSPropertyAttributeNone},
        {nullptr, nullptr, 0},
    };

    JSClassDefinition baseDef = kJSClassDefinitionEmpty;
    baseDef.className = "BaseWidget";
    baseDef.staticValues = baseStaticValues;
    baseDef.staticFunctions = baseStaticFunctions;
    baseDef.initialize = &BaseInitCallback;
    baseDef.finalize = &BaseFinalizeCallback;
    JSClassRef baseClass = JSClassCreate(&baseDef);
    Require(baseClass != nullptr);
    Require(JSClassRetain(baseClass) == baseClass);
    JSClassRelease(baseClass);

    JSClassDefinition derivedDef = kJSClassDefinitionEmpty;
    derivedDef.className = "DerivedWidget";
    derivedDef.parentClass = baseClass;
    derivedDef.initialize = &DerivedInitCallback;
    derivedDef.finalize = &DerivedFinalizeCallback;
    derivedDef.hasProperty = &DynamicHasProperty;
    derivedDef.getProperty = &DynamicGetProperty;
    derivedDef.setProperty = &DynamicSetProperty;
    derivedDef.deleteProperty = &DynamicDeleteProperty;
    derivedDef.getPropertyNames = &DynamicGetPropertyNames;
    derivedDef.callAsFunction = &ClassCallAsFunction;
    derivedDef.callAsConstructor = &ClassCallAsConstructor;
    derivedDef.hasInstance = &ClassHasInstance;
    derivedDef.convertToType = &ClassConvertToType;
    JSClassRef derivedClass = JSClassCreate(&derivedDef);
    Require(derivedClass != nullptr);

    ClassTestState state;
    JSObjectRef widget = JSObjectMake(ctx, derivedClass, &state);
    Require(widget != nullptr);
    Require(JSObjectGetPrivate(widget) == &state);
    Require(state.initOrder.size() == 2 && state.initOrder[0] == 1 && state.initOrder[1] == 2);
    Require(JSValueIsObjectOfClass(ctx, widget, derivedClass));
    Require(JSValueIsObjectOfClass(ctx, widget, baseClass));

    ScopedJsString privKey("secret");
    Require(JSObjectGetPrivateProperty(ctx, widget, privKey) == nullptr);
    Require(JSObjectSetPrivateProperty(ctx, widget, privKey, JSValueMakeNumber(ctx, 1337.0)));
    Require(JSValueToNumber(ctx, JSObjectGetPrivateProperty(ctx, widget, privKey), nullptr) == 1337.0);
    Require(JSObjectDeletePrivateProperty(ctx, widget, privKey));
    Require(JSObjectGetPrivateProperty(ctx, widget, privKey) == nullptr);

    ScopedJsString widgetGlobal("widget");
    JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), widgetGlobal, widget, kJSPropertyAttributeNone, nullptr);

    Require(JSValueToNumber(ctx, EvalUtf8(ctx, "widget.staticVal"), nullptr) == 10.0);
    EvalUtf8(ctx, "widget.staticVal = 25;");
    Require(state.staticVal == 25.0);
    Require(JSValueToNumber(ctx, EvalUtf8(ctx, "widget.multiply(3)"), nullptr) == 75.0);
    EvalUtf8(ctx, "widget.readOnlyStatic = 0;");
    Require(JSValueToNumber(ctx, EvalUtf8(ctx, "widget.readOnlyStatic"), nullptr) == 999.0);
    Require(!JSValueToBoolean(ctx, EvalUtf8(ctx, "delete widget.staticVal")));

    Require(JSValueToBoolean(ctx, EvalUtf8(ctx, "'dynProp' in widget")));
    Require(JSValueToNumber(ctx, EvalUtf8(ctx, "widget.dynProp"), nullptr) == 77.0);
    EvalUtf8(ctx, "widget.dynProp = 88;");
    Require(state.dynamicVal == 88.0);
    Require(JSValueToBoolean(ctx, EvalUtf8(ctx, "delete widget.dynProp")));
    Require(!JSValueToBoolean(ctx, EvalUtf8(ctx, "'dynProp' in widget")));
    EvalUtf8(ctx, "widget.dynProp = 99;");
    Require(JSValueToBoolean(ctx, EvalUtf8(ctx, "'dynProp' in widget")));

    JSValueRef getExc = nullptr;
    ScopedJsString throwProp("throwProp");
    Require(JSObjectGetProperty(ctx, widget, throwProp, &getExc) == nullptr);
    Require(getExc != nullptr);

    state.throwOnSet = true;
    JSValueRef setExc = nullptr;
    ScopedJsString dynProp("dynProp");
    JSObjectSetProperty(ctx, widget, dynProp, JSValueMakeNumber(ctx, 1.0), kJSPropertyAttributeNone, &setExc);
    Require(setExc != nullptr);
    state.throwOnSet = false;

    JSPropertyNameArrayRef propNames = JSObjectCopyPropertyNames(ctx, widget);
    Require(propNames != nullptr);
    Require(JSPropertyNameArrayRetain(propNames) == propNames);
    JSPropertyNameArrayRelease(propNames);
    const std::size_t nameCount = JSPropertyNameArrayGetCount(propNames);
    bool sawStaticVal = false;
    bool sawMultiply = false;
    bool sawDynProp = false;
    bool sawEnumeratedExtra = false;
    bool sawReadOnlyStatic = false;
    for (std::size_t i = 0; i < nameCount; ++i) {
        JSStringRef name = JSPropertyNameArrayGetNameAtIndex(propNames, i);
        if (JSStringIsEqualToUTF8CString(name, "staticVal")) sawStaticVal = true;
        if (JSStringIsEqualToUTF8CString(name, "multiply")) sawMultiply = true;
        if (JSStringIsEqualToUTF8CString(name, "dynProp")) sawDynProp = true;
        if (JSStringIsEqualToUTF8CString(name, "enumeratedExtra")) sawEnumeratedExtra = true;
        if (JSStringIsEqualToUTF8CString(name, "readOnlyStatic")) sawReadOnlyStatic = true;
    }
    Require(sawStaticVal && sawMultiply && sawDynProp && sawEnumeratedExtra && !sawReadOnlyStatic);
    JSPropertyNameArrayRelease(propNames);

    Require(JSObjectIsFunction(ctx, widget) && JSObjectIsConstructor(ctx, widget));
    Require(JSValueToNumber(ctx, EvalUtf8(ctx, "widget(5, 10)"), nullptr) == 40.0);
    Require(JSValueToNumber(ctx, EvalUtf8(ctx, "(new widget(123)).constructedVal"), nullptr) == 123.0);
    Require(JSValueToBoolean(ctx, EvalUtf8(ctx, "({customInstanceMarker: 1}) instanceof widget")));
    Require(!JSValueToBoolean(ctx, EvalUtf8(ctx, "({}) instanceof widget")));
    Require(JSValueToNumber(ctx, EvalUtf8(ctx, "+widget"), nullptr) == 30.0);
    JSStringRef toStr = JSValueToStringCopy(ctx, EvalUtf8(ctx, "`${widget}`"), nullptr);
    Require(JSStringIsEqualToUTF8CString(toStr, "converted-string"));
    JSStringRelease_nid_postfix(toStr);

    JSObjectRef ctorObj = JSObjectMakeConstructor(ctx, baseClass, nullptr);
    Require(JSObjectIsConstructor(ctx, ctorObj) && !JSObjectIsFunction(ctx, ctorObj));
    JSObjectRef constructedBase = JSObjectCallAsConstructor(ctx, ctorObj, 0, nullptr, nullptr);
    Require(JSValueIsObjectOfClass(ctx, constructedBase, baseClass));

    JSClassDefinition noProtoDef = kJSClassDefinitionEmpty;
    noProtoDef.attributes = kJSClassAttributeNoAutomaticPrototype;
    noProtoDef.staticFunctions = baseStaticFunctions;
    JSClassRef noProtoClass = JSClassCreate(&noProtoDef);
    JSObjectRef npObj = JSObjectMake(ctx, noProtoClass, &state);
    JSObjectRef plainObj = JSObjectMake(ctx, nullptr, nullptr);
    Require(JSValueIsStrictEqual(ctx, JSObjectGetPrototype(ctx, npObj), JSObjectGetPrototype(ctx, plainObj)));
    JSClassRelease(noProtoClass);

    JSObjectDeleteProperty(ctx, JSContextGetGlobalObject(ctx), widgetGlobal, nullptr);
    JSGarbageCollect(ctx);
    Require(state.finalizeOrder.size() == 2 && state.finalizeOrder[0] == 2 && state.finalizeOrder[1] == 1);

    JSClassRelease(derivedClass);
    JSClassRelease(baseClass);
    JSGlobalContextRelease(ctx);

    JSGlobalContextRef customGlobalCtx = JSGlobalContextCreate(baseClass = JSClassCreate(&baseDef));
    ClassTestState globalState;
    globalState.staticVal = 50.0;
    JSObjectSetPrivate(JSContextGetGlobalObject(customGlobalCtx), &globalState);
    Require(JSValueToNumber(customGlobalCtx, EvalUtf8(customGlobalCtx, "multiply(2)"), nullptr) == 100.0);
    JSGlobalContextRelease(customGlobalCtx);
    JSClassRelease(baseClass);
}

void TestTypedArraysAndArrayBuffers() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);

    JSObjectRef u32Arr = JSObjectMakeTypedArray(ctx, kJSTypedArrayTypeUint32Array, 4, nullptr);
    Require(u32Arr != nullptr);
    Require(JSValueGetTypedArrayType(ctx, u32Arr, nullptr) == kJSTypedArrayTypeUint32Array);
    Require(JSObjectGetTypedArrayLength(ctx, u32Arr, nullptr) == 4);
    Require(JSObjectGetTypedArrayByteLength(ctx, u32Arr, nullptr) == 16);
    Require(JSObjectGetTypedArrayByteOffset(ctx, u32Arr, nullptr) == 0);
    auto* u32Ptr = static_cast<std::uint32_t*>(JSObjectGetTypedArrayBytesPtr(ctx, u32Arr, nullptr));
    Require(u32Ptr != nullptr);
    u32Ptr[0] = 11;
    u32Ptr[1] = 22;
    u32Ptr[2] = 33;
    u32Ptr[3] = 44;
    Require(JSValueToNumber(ctx, JSObjectGetPropertyAtIndex(ctx, u32Arr, 2, nullptr), nullptr) == 33.0);

    JSObjectRef backingBuf = JSObjectGetTypedArrayBuffer(ctx, u32Arr, nullptr);
    Require(backingBuf != nullptr);
    Require(JSValueGetTypedArrayType(ctx, backingBuf, nullptr) == kJSTypedArrayTypeArrayBuffer);
    Require(JSObjectGetArrayBufferBytesPtr(ctx, backingBuf, nullptr) == u32Ptr);
    Require(JSObjectGetArrayBufferByteLength(ctx, backingBuf, nullptr) == 16);

    JSObjectRef subView = JSObjectMakeTypedArrayWithArrayBufferAndOffset(ctx, kJSTypedArrayTypeUint16Array, backingBuf, 4, 4, nullptr);
    Require(subView != nullptr);
    Require(JSObjectGetTypedArrayByteOffset(ctx, subView, nullptr) == 4);
    Require(JSObjectGetTypedArrayLength(ctx, subView, nullptr) == 4);
    Require(JSObjectGetTypedArrayByteLength(ctx, subView, nullptr) == 8);
    Require(JSObjectGetTypedArrayBytesPtr(ctx, subView, nullptr) == u32Ptr);

    JSObjectRef fullView = JSObjectMakeTypedArrayWithArrayBuffer(ctx, kJSTypedArrayTypeUint8Array, backingBuf, nullptr);
    Require(fullView != nullptr);
    Require(JSObjectGetTypedArrayLength(ctx, fullView, nullptr) == 16);

    int deallocCount = 0;
    auto* rawBytes = static_cast<float*>(std::malloc(sizeof(float) * 2));
    rawBytes[0] = 1.5f;
    rawBytes[1] = -2.5f;
    JSObjectRef noCopyTa = JSObjectMakeTypedArrayWithBytesNoCopy(ctx, kJSTypedArrayTypeFloat32Array, rawBytes, sizeof(float) * 2, &NoCopyDeallocator, &deallocCount, nullptr);
    Require(noCopyTa != nullptr);
    Require(JSObjectGetTypedArrayBytesPtr(ctx, noCopyTa, nullptr) == rawBytes);
    Require(JSValueToNumber(ctx, JSObjectGetPropertyAtIndex(ctx, noCopyTa, 0, nullptr), nullptr) == 1.5);
    Require(JSValueToNumber(ctx, JSObjectGetPropertyAtIndex(ctx, noCopyTa, 1, nullptr), nullptr) == -2.5);

    JSGarbageCollect(ctx);
    Require(deallocCount == 1);

    JSGlobalContextRelease(ctx);
}

void TestGcProtectWeakAndWatchdog() {
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef ctx = JSGlobalContextCreateInGroup(group, nullptr);

    int heapFinalizerCount = 0;
    JSContextGroupAddHeapFinalizer(group, &HeapFinalizerCallback, &heapFinalizerCount);
    JSReportExtraMemoryCost(ctx, 4096);
    JSDisableGCTimer();

    JSClassDefinition def = kJSClassDefinitionEmpty;
    def.className = "TrackedObj";
    def.finalize = &BaseFinalizeCallback;
    JSClassRef cls = JSClassCreate(&def);

    ClassTestState st1;
    JSObjectRef obj1 = JSObjectMake(ctx, cls, &st1);
    JSWeakRef weak1 = JSWeakCreate(group, obj1);
    JSWeakRetain(weak1);
    JSWeakRelease(weak1);
    Require(JSWeakGetObject(weak1) == obj1);

    bool mapDestroyed = false;
    auto* weakMap = new OpaqueJSWeakObjectMap();
    weakMap->group = const_cast<OpaqueJSContextGroup*>(group);
    weakMap->privateData = &mapDestroyed;
    weakMap->destroyedCallback = &WeakMapDestroyed;
    const_cast<OpaqueJSContextGroup*>(group)->weakMaps.push_back(weakMap);

    int key1 = 1;
    int key2 = 2;
    JSWeakObjectMapSet(ctx, weakMap, &key1, obj1);
    Require(JSWeakObjectMapGet(ctx, weakMap, &key1) == obj1);
    JSWeakObjectMapSet(ctx, weakMap, &key2, obj1);
    Require(JSWeakObjectMapRemove(ctx, weakMap, &key2));
    Require(!JSWeakObjectMapRemove(ctx, weakMap, &key2));

    JSValueProtect(ctx, obj1);
    JSGarbageCollect(ctx);
    Require(heapFinalizerCount == 1);
    Require(st1.finalizeOrder.empty());
    Require(JSWeakGetObject(weak1) == obj1);
    Require(JSWeakObjectMapGet(ctx, weakMap, &key1) == obj1);

    ConstraintState cst;
    ClassTestState st2;
    cst.parentObj = obj1;
    cst.childObj = JSObjectMake(ctx, cls, &st2);
    JSContextGroupAddMarkingConstraint(group, &MarkingConstraintCallback, &cst);

    JSSynchronousEdenCollectForDebugging(ctx);
    Require(st2.finalizeOrder.empty());

    JSObjectRef stats = JSGetMemoryUsageStatistics(ctx);
    Require(stats != nullptr);
    ScopedJsString extraMemKey("extraMemorySize");
    Require(JSValueToNumber(ctx, JSObjectGetProperty(ctx, stats, extraMemKey, nullptr), nullptr) == 4096.0);

    JSValueUnprotect(ctx, obj1);
    JSSynchronousGarbageCollectForDebugging(ctx);
    Require(st1.finalizeOrder.size() == 1);
    Require(st2.finalizeOrder.size() == 1);
    Require(JSWeakGetObject(weak1) == nullptr);
    Require(JSWeakObjectMapGet(ctx, weakMap, &key1) == nullptr);

    JSContextGroupRemoveHeapFinalizer(group, &HeapFinalizerCallback, &heapFinalizerCount);
    const int beforeFinalizerCount = heapFinalizerCount;
    JSGarbageCollect(ctx);
    Require(heapFinalizerCount == beforeFinalizerCount);

    delete weakMap;
    Require(mapDestroyed);
    JSWeakRelease(weak1);
    JSClassRelease(cls);

    int watchdogCalls = 0;
    JSContextGroupSetExecutionTimeLimit(group, 0.001, &ShouldTerminateWatchdog, &watchdogCalls);
    JSValueRef loopExc = nullptr;
    JSValueRef loopRes = EvalUtf8(ctx, "while (true) {}", &loopExc);
    Require(loopRes == nullptr);
    Require(watchdogCalls == 2);
    JSContextGroupClearExecutionTimeLimit(group);
    Require(JSValueToNumber(ctx, EvalUtf8(ctx, "2 + 2"), nullptr) == 4.0);

    JSGlobalContextRelease(ctx);
    JSContextGroupRelease(group);
}

}  // namespace

int main() {
    TestStringsAndOpaqueDestructors();
    TestContextsAndScripts();
    TestValuesAndObjects();
    TestClassesAndCallbacks();
    TestTypedArraysAndArrayBuffers();
    TestGcProtectWeakAndWatchdog();
    return 0;
}
