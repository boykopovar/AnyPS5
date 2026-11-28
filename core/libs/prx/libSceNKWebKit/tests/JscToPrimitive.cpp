#include "../src/Jsc.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceJsc/include/Api.hpp"
#include "prx/libSceJsc/include/ApiBridgeExports.hpp"
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>

using namespace NKWebKit;

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JSC to primitive check failed at line %u\n", location.line());
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

bool StringCopyEquals(JSContextRef ctx, JSValueRef value, const char* expected) {
    JSValueRef exception = nullptr;
    JSStringRef text = JSValueToStringCopy(ctx, value, &exception);
    if (text == nullptr || exception != nullptr) return false;
    const bool equal = JSStringIsEqualToUTF8CString(text, expected);
    JSStringRelease_nid_postfix(text);
    return equal;
}

double numberAtInitialize = 0.0;

JSValueRef APS5_VABI ConvertOneOrText(JSContextRef ctx, JSObjectRef object, JSType type, JSValueRef* exception) {
    (void)object;
    (void)exception;
    if (type == kJSTypeString) {
        ScopedJsString text("AsString");
        return static_cast<JSValueRef>(JSValueMakeString(ctx, text));
    }
    return JSValueMakeNumber(ctx, 1.0);
}

JSValueRef APS5_VABI ConvertSeven(JSContextRef ctx, JSObjectRef object, JSType type, JSValueRef* exception) {
    (void)object;
    (void)type;
    (void)exception;
    return JSValueMakeNumber(ctx, 7.0);
}

JSValueRef APS5_VABI ConvertDeclines(JSContextRef ctx, JSObjectRef object, JSType type, JSValueRef* exception) {
    (void)ctx;
    (void)object;
    (void)type;
    (void)exception;
    return nullptr;
}

void APS5_VABI ConvertAtInitialize(JSContextRef ctx, JSObjectRef object) {
    numberAtInitialize = JSValueToNumber(ctx, object, nullptr);
}

JSValueRef APS5_VABI ReturnUndefined(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)function;
    (void)thisObject;
    (void)argumentCount;
    (void)arguments;
    (void)exception;
    return JSValueMakeUndefined(ctx);
}

JSClassRef MakeClass(JSObjectConvertToTypeCallback convertToType, JSClassRef parentClass = nullptr, JSObjectInitializeCallback initialize = nullptr) {
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.convertToType = convertToType;
    definition.parentClass = parentClass;
    definition.initialize = initialize;
    JSClassRef jsClass = JSClassCreate(&definition);
    Require(jsClass != nullptr);
    return jsClass;
}

const char* const ErrorMessage =
    "function messageOf(run) { try { run(); } catch (error) { return error instanceof TypeError ? error.message : 'not a TypeError'; } return 'no error'; }"
    "var wrongThis = 'JSCallbackObject[Symbol.toPrimitive] method called on incompatible |this| value.';"
    "var notAString = 'Primitive hint is not a string.';"
    "var unknownHint = \"Expected primitive hint to match one of 'default', 'number', 'string'.\"; true";

void TestTheProperty(JSContextRef ctx) {
    Require(EvalBool(ctx, "var d = Object.getOwnPropertyDescriptor(conv, Symbol.toPrimitive);"
                          "typeof d === 'object' && d.value === conv[Symbol.toPrimitive] && d.writable === true && d.enumerable === false && d.configurable === true"));
    Require(EvalBool(ctx, "var method = conv[Symbol.toPrimitive]; typeof method === 'function' && method === conv[Symbol.toPrimitive] && method.name === '[Symbol.toPrimitive]' && method.length === 1"));
    Require(EvalBool(ctx, "Object.prototype.hasOwnProperty.call(conv, Symbol.toPrimitive) && Object.getOwnPropertySymbols(conv).indexOf(Symbol.toPrimitive) >= 0"));
    Require(EvalBool(ctx, "conv[Symbol.toPrimitive] !== seven[Symbol.toPrimitive]"));
    Require(EvalBool(ctx, "plain[Symbol.toPrimitive] === undefined && Object.getOwnPropertyDescriptor(plain, Symbol.toPrimitive) === undefined"));
    Require(EvalBool(ctx, "!Object.prototype.hasOwnProperty.call(Object.getPrototypeOf(conv), Symbol.toPrimitive)"));
    Require(EvalBool(ctx, "typeof child[Symbol.toPrimitive] === 'function' && child[Symbol.toPrimitive]('number') === 1"));
}

void TestTheCall(JSContextRef ctx) {
    Require(EvalBool(ctx, "conv[Symbol.toPrimitive]('default') === 1 && conv[Symbol.toPrimitive]('number') === 1 && conv[Symbol.toPrimitive]('string') === 'AsString'"));
    Require(EvalBool(ctx, "messageOf(function () { conv[Symbol.toPrimitive]('foo'); }) === unknownHint"));
    Require(EvalBool(ctx, "messageOf(function () { conv[Symbol.toPrimitive]('Number'); }) === unknownHint"));
    Require(EvalBool(ctx, "messageOf(function () { conv[Symbol.toPrimitive](); }) === notAString"));
    Require(EvalBool(ctx, "messageOf(function () { conv[Symbol.toPrimitive](1); }) === notAString"));
    Require(EvalBool(ctx, "messageOf(function () { conv[Symbol.toPrimitive](new String('number')); }) === notAString"));
    Require(EvalBool(ctx, "messageOf(function () { conv[Symbol.toPrimitive].call({}, 'default'); }) === wrongThis"));
    Require(EvalBool(ctx, "messageOf(function () { (0, conv[Symbol.toPrimitive])('default'); }) === wrongThis"));
    Require(EvalBool(ctx, "messageOf(function () { conv[Symbol.toPrimitive].call(1, 'default'); }) === wrongThis"));
    Require(EvalBool(ctx, "messageOf(function () { conv[Symbol.toPrimitive].call(this, 'default'); }) === wrongThis"));
    Require(EvalBool(ctx, "messageOf(function () { conv[Symbol.toPrimitive].call(apiFunction, 'default'); }) === wrongThis"));
    Require(EvalBool(ctx, "messageOf(function () { conv[Symbol.toPrimitive].call(apiConstructor, 'default'); }) === wrongThis"));
    Require(EvalBool(ctx, "messageOf(function () { conv[Symbol.toPrimitive].call({}, 'foo'); }) === wrongThis"));
    Require(EvalBool(ctx, "conv[Symbol.toPrimitive].call(seven, 'string') === 7 && seven[Symbol.toPrimitive].call(conv, 'string') === 'AsString'"));
    Require(EvalBool(ctx, "plain.toString = function () { return 'plainText'; }; conv[Symbol.toPrimitive].call(plain, 'string') === 'plainText' && conv[Symbol.toPrimitive].call(plain, 'number') === 'plainText'"));
    Require(EvalBool(ctx, "plain.valueOf = function () { return 3; }; conv[Symbol.toPrimitive].call(plain, 'default') === 3 && conv[Symbol.toPrimitive].call(plain, 'string') === 'plainText'"));
    Require(EvalBool(ctx, "declines.valueOf = function () { return 5; }; declines[Symbol.toPrimitive]('number') === 5"));
}

void TestConversions(JSContextRef ctx) {
    Require(EvalBool(ctx, "conv + 1 === 2 && `${conv}` === 'AsString' && Number(conv) === 1 && seven * 2 === 14 && declines + 1 === 6"));
    JSValueRef conv = Eval(ctx, "conv");
    Require(JSValueToNumber(ctx, conv, nullptr) == 1.0);
    Require(StringCopyEquals(ctx, conv, "AsString"));
}

void TestTheMethodOutlivesItsObject(JSContextRef ctx, JSClassRef convClass) {
    SetGlobal(ctx, "shortLived", JSObjectMake(ctx, convClass, nullptr));
    Eval(ctx, "var kept = shortLived[Symbol.toPrimitive]; shortLived = null");
    ScopedJsString name("shortLived");
    JSObjectDeleteProperty(ctx, JSContextGetGlobalObject(ctx), name, nullptr);
    JSGarbageCollect(ctx);
    Require(EvalBool(ctx, "kept.call(conv, 'number') === 1 && kept.call(seven, 'number') === 7"));
    Require(EvalBool(ctx, "messageOf(function () { kept('number'); }) === wrongThis"));
}

void TestThePropertyCanBeReplaced(JSContextRef ctx) {
    Require(EvalBool(ctx, "conv[Symbol.toPrimitive] = function () { return null; }; conv[Symbol.toPrimitive]('bar') === null && String(conv) === 'null'"));
    JSValueRef conv = Eval(ctx, "conv");
    Require(JSValueToNumber(ctx, conv, nullptr) == 0.0);
    Require(StringCopyEquals(ctx, conv, "null"));
    Require(EvalBool(ctx, "delete conv[Symbol.toPrimitive] && conv[Symbol.toPrimitive] === undefined && !(Symbol.toPrimitive in conv)"));
    Require(EvalBool(ctx, "conv.valueOf = function () { return 9; }; conv + 1 === 10"));
    Require(JSValueToNumber(ctx, conv, nullptr) == 9.0);
}

void TestCallbackObjects() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    JSClassRef convClass = MakeClass(&ConvertOneOrText);
    JSClassRef sevenClass = MakeClass(&ConvertSeven);
    JSClassRef declinesClass = MakeClass(&ConvertDeclines);
    JSClassRef plainClass = MakeClass(nullptr);
    JSClassRef childClass = MakeClass(nullptr, convClass);
    JSClassRef initializedClass = MakeClass(&ConvertSeven, nullptr, &ConvertAtInitialize);
    SetGlobal(ctx, "conv", JSObjectMake(ctx, convClass, nullptr));
    SetGlobal(ctx, "seven", JSObjectMake(ctx, sevenClass, nullptr));
    SetGlobal(ctx, "declines", JSObjectMake(ctx, declinesClass, nullptr));
    SetGlobal(ctx, "plain", JSObjectMake(ctx, plainClass, nullptr));
    SetGlobal(ctx, "child", JSObjectMake(ctx, childClass, nullptr));
    SetGlobal(ctx, "apiFunction", JSObjectMakeFunctionWithCallback(ctx, nullptr, &ReturnUndefined));
    SetGlobal(ctx, "apiConstructor", JSObjectMakeConstructor(ctx, convClass, nullptr));
    Require(EvalBool(ctx, ErrorMessage));

    numberAtInitialize = 0.0;
    JSObjectMake(ctx, initializedClass, nullptr);
    Require(numberAtInitialize == 7.0);

    TestTheProperty(ctx);
    TestTheCall(ctx);
    TestConversions(ctx);
    TestTheMethodOutlivesItsObject(ctx, convClass);
    TestThePropertyCanBeReplaced(ctx);

    JSClassRelease(initializedClass);
    JSClassRelease(childClass);
    JSClassRelease(plainClass);
    JSClassRelease(declinesClass);
    JSClassRelease(sevenClass);
    JSClassRelease(convClass);
    JSGlobalContextRelease(ctx);
}

void TestAGlobalObjectWithAClass() {
    JSClassRef globalClass = MakeClass(&ConvertOneOrText, nullptr, &ConvertAtInitialize);
    JSClassRef convClass = MakeClass(&ConvertSeven);
    numberAtInitialize = 0.0;
    JSGlobalContextRef ctx = JSGlobalContextCreate(globalClass);
    Require(numberAtInitialize == 1.0);
    SetGlobal(ctx, "seven", JSObjectMake(ctx, convClass, nullptr));
    Require(EvalBool(ctx, ErrorMessage));
    Require(EvalBool(ctx, "var d = Object.getOwnPropertyDescriptor(this, Symbol.toPrimitive);"
                          "typeof d === 'object' && d.value === this[Symbol.toPrimitive] && d.writable === true && d.enumerable === false && d.configurable === true"));
    Require(EvalBool(ctx, "this[Symbol.toPrimitive]('number') === 1 && this[Symbol.toPrimitive]('string') === 'AsString' && this + 1 === 2 && `${this}` === 'AsString'"));
    Require(EvalBool(ctx, "var globalMethod = this[Symbol.toPrimitive]; messageOf(function () { globalMethod.call(seven, 'number'); }) === wrongThis"));
    Require(EvalBool(ctx, "var globalObject = this; messageOf(function () { seven[Symbol.toPrimitive].call(globalObject, 'number'); }) === wrongThis"));
    Require(EvalBool(ctx, "messageOf(function () { globalMethod.call(Object.getPrototypeOf(globalObject), 'number'); }) === wrongThis"));
    Require(EvalBool(ctx, "this[Symbol.toPrimitive] = function () { return 4; }; this + 1 === 5"));
    Require(JSValueToNumber(ctx, JSContextGetGlobalObject(ctx), nullptr) == 4.0);
    JSGlobalContextRelease(ctx);
    JSClassRelease(convClass);
    JSClassRelease(globalClass);

    JSGlobalContextRef plainCtx = JSGlobalContextCreate(nullptr);
    Require(EvalBool(plainCtx, "this[Symbol.toPrimitive] === undefined"));
    JSGlobalContextRelease(plainCtx);
}

}  // namespace

int main() {
    TestCallbackObjects();
    TestAGlobalObjectWithAClass();
    std::puts("libSceNKWebKit JS to primitive tests passed");
    return 0;
}
