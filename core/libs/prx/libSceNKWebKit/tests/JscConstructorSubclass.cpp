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
    std::fprintf(stderr, "JSC constructor subclass check failed at line %u\n", location.line());
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

enum Made {
    kOfTheClass,
    kPlain,
    kAnArray,
    kAProxy,
    kNotExtensible,
    kTheGlobalObject,
};

JSClassRef theClass = nullptr;
Made made = kOfTheClass;
int constructed = 0;

JSValueRef APS5_VABI NativeMethod(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)function;
    (void)thisObject;
    (void)argumentCount;
    (void)arguments;
    (void)exception;
    return JSValueMakeNumber(ctx, 11.0);
}

JSObjectRef APS5_VABI Construct(JSContextRef ctx, JSObjectRef constructor, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)constructor;
    (void)argumentCount;
    (void)arguments;
    (void)exception;
    ++constructed;
    Eval(ctx, "log.push('constructed')");
    if (made == kPlain) return JSObjectMake(ctx, nullptr, nullptr);
    if (made == kAnArray) return JSObjectMakeArray(ctx, 0, nullptr, nullptr);
    if (made == kTheGlobalObject) return JSContextGetGlobalObject(ctx);
    JSObjectRef object = JSObjectMake(ctx, theClass, nullptr);
    if (made == kOfTheClass) return object;
    SetGlobal(ctx, "justMade", object);
    if (made == kNotExtensible) return JSValueToObject(ctx, Eval(ctx, "Object.preventExtensions(justMade)"), nullptr);
    return JSValueToObject(ctx, Eval(ctx, "new Proxy(justMade, { getPrototypeOf: function () { trapped = true; return Object.getPrototypeOf(justMade); } })"), nullptr);
}

JSObjectRef APS5_VABI ConstructByTheClass(JSContextRef ctx, JSObjectRef constructor, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)constructor;
    (void)argumentCount;
    (void)arguments;
    (void)exception;
    return JSObjectMake(ctx, nullptr, nullptr);
}

const char* const kSubclass = "class Subclass extends Superclass { method() { return 'value'; } }";

void TestASubclassOfAClassConstructor(JSContextRef ctx) {
    made = kOfTheClass;
    Require(EvalBool(ctx, "var made = new Subclass(); Object.getPrototypeOf(made) === Subclass.prototype"));
    Require(EvalBool(ctx, "made.method() === 'value' && made.native() === 11"));
    Require(EvalBool(ctx, "made instanceof Subclass && made instanceof Superclass"));
    JSObjectRef object = JSValueToObject(ctx, Eval(ctx, "made"), nullptr);
    Require(JSValueIsObjectOfClass(ctx, object, theClass));
    ScopedJsString method("method");
    Require(JSObjectHasProperty(ctx, object, method));

    Require(EvalBool(ctx, "var direct = new Superclass(); Object.getPrototypeOf(direct) === Superclass.prototype && direct.method === undefined && direct.native() === 11"));
    Require(EvalBool(ctx, "class Deeper extends Subclass { deeper() { return 3; } } var deep = new Deeper();"
                          "Object.getPrototypeOf(deep) === Deeper.prototype && deep.deeper() === 3 && deep.method() === 'value' && deep.native() === 11"));
    Require(EvalBool(ctx, "function Other() {} Other.prototype = { tag: 'other' };"
                          "var reflected = Reflect.construct(Superclass, [], Other); Object.getPrototypeOf(reflected) === Other.prototype && reflected.tag === 'other'"));
}

void TestThePrototypeTheNewTargetGives(JSContextRef ctx) {
    made = kOfTheClass;
    Require(EvalBool(ctx, "function NoPrototype() {} NoPrototype.prototype = null; Object.getPrototypeOf(Reflect.construct(Superclass, [], NoPrototype)) === null"));
    Require(EvalBool(ctx, "function Primitive() {} Primitive.prototype = 5; Object.getPrototypeOf(Reflect.construct(Superclass, [], Primitive)) === Superclass.prototype"));
    Require(EvalBool(ctx, "var bound = function () {}.bind(null); Object.getPrototypeOf(Reflect.construct(Superclass, [], bound)) === Superclass.prototype"));

    Require(EvalBool(ctx, "log = []; var watched = new Proxy(function () {}, { get: function (target, key) { log.push('get ' + String(key)); return target[key]; } });"
                          "Reflect.construct(Superclass, [], watched); log.join() === 'get prototype,constructed'"));
    constructed = 0;
    Require(EvalBool(ctx, "var failing = new Proxy(function () {}, { get: function () { throw new RangeError('no prototype'); } });"
                          "var thrown = null; try { Reflect.construct(Superclass, [], failing); } catch (e) { thrown = e; } thrown instanceof RangeError"));
    Require(constructed == 0);
}

void TestWhatTheCallbackReturns(JSContextRef ctx) {
    made = kAnArray;
    Require(EvalBool(ctx, "var list = new Subclass(); Object.getPrototypeOf(list) === Array.prototype && list.method === undefined"));

    made = kPlain;
    Require(EvalBool(ctx, "var plain = new Subclass(); Object.getPrototypeOf(plain) === Object.prototype && plain.method === undefined"));

    made = kAProxy;
    Require(EvalBool(ctx, "var trapped = false; var proxied = new Subclass(); trapped === false"));
    Require(EvalBool(ctx, "Object.getPrototypeOf(proxied) === Superclass.prototype && trapped === true"));

    made = kNotExtensible;
    Require(EvalBool(ctx, "var sealed = new Subclass(); Object.getPrototypeOf(sealed) === Superclass.prototype && sealed.method === undefined"));
    JSObjectRef buffer = JSValueToObject(ctx, Eval(ctx, "new ArrayBuffer(8)"), nullptr);
    JSValueRef exception = nullptr;
    Require(JSObjectGetArrayBufferBytesPtr(ctx, buffer, &exception) != nullptr && exception == nullptr);
}

void TestAConstructorWithoutAClass(JSContextRef ctx) {
    SetGlobal(ctx, "Plain", JSObjectMakeConstructor(ctx, nullptr, &Construct));
    made = kPlain;
    Require(EvalBool(ctx, "class OfPlain extends Plain { method() { return 'plain'; } } var ofPlain = new OfPlain();"
                          "Object.getPrototypeOf(ofPlain) === OfPlain.prototype && ofPlain.method() === 'plain' && ofPlain instanceof Plain"));
    made = kOfTheClass;
    Require(EvalBool(ctx, "var foreign = new OfPlain(); Object.getPrototypeOf(foreign) === Superclass.prototype"));

    made = kTheGlobalObject;
    Require(EvalBool(ctx, "var before = Object.getPrototypeOf(globalThis); var same = new OfPlain(); same === globalThis && Object.getPrototypeOf(globalThis) === before"));
}

void TestTheConstructorsThatDoNotAdopt(JSContextRef ctx) {
    SetGlobal(ctx, "NoCallback", JSObjectMakeConstructor(ctx, theClass, nullptr));
    Require(EvalBool(ctx, "class OfNoCallback extends NoCallback { method() { return 1; } } var bare = new OfNoCallback();"
                          "Object.getPrototypeOf(bare) === NoCallback.prototype && bare.method === undefined && bare.native() === 11"));

    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.callAsConstructor = &ConstructByTheClass;
    JSClassRef constructible = JSClassCreate(&definition);
    SetGlobal(ctx, "constructible", JSObjectMake(ctx, constructible, nullptr));
    JSClassRelease(constructible);
    Require(EvalBool(ctx, "function Target() {} var built = Reflect.construct(constructible, [], Target); Object.getPrototypeOf(built) === Object.prototype"));
}

}  // namespace

int main() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    const JSStaticFunction staticFunctions[] = {
        {"native", &NativeMethod, kJSPropertyAttributeNone},
        {nullptr, nullptr, 0},
    };
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    definition.className = "Native";
    definition.staticFunctions = staticFunctions;
    theClass = JSClassCreate(&definition);
    SetGlobal(ctx, "Superclass", JSObjectMakeConstructor(ctx, theClass, &Construct));
    Eval(ctx, "var log = []; 0");
    Eval(ctx, kSubclass);
    TestASubclassOfAClassConstructor(ctx);
    TestThePrototypeTheNewTargetGives(ctx);
    TestWhatTheCallbackReturns(ctx);
    TestAConstructorWithoutAClass(ctx);
    TestTheConstructorsThatDoNotAdopt(ctx);
    JSClassRelease(theClass);
    JSGlobalContextRelease(ctx);
    std::puts("libSceNKWebKit JS constructor subclass tests passed");
    return 0;
}
