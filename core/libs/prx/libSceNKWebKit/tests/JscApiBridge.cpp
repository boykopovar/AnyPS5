#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"

#include <cstdio>
#include <cstdlib>

using namespace NKWebKit;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "JscApiBridge check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct ScopedJsString {
    JSStringRef ref = nullptr;
    explicit ScopedJsString(const char* utf8) : ref(JSStringCreateWithUTF8CString_nid_postfix(utf8)) {}
    explicit ScopedJsString(JSStringRef r) : ref(r) {}
    ~ScopedJsString() {
        if (ref) JSStringRelease_nid_postfix(ref);
    }
    ScopedJsString(const ScopedJsString&) = delete;
    ScopedJsString& operator=(const ScopedJsString&) = delete;
    operator JSStringRef() const { return ref; }
};

JSValueRef Eval(JSContextRef ctx, const char* code) {
    ScopedJsString script(code);
    JSValueRef exception = nullptr;
    JSValueRef result = JSEvaluateScript(ctx, script, nullptr, nullptr, 1, &exception);
    Require(exception == nullptr);
    return result;
}

JSValueRef MakeString(JSContextRef ctx, JSStringRef string) {
    return static_cast<JSValueRef>(JSValueMakeString(ctx, string));
}

bool HasPropertyForKey(JSContextRef ctx, JSObjectRef object, JSValueRef key, JSValueRef* exception) {
    return JSObjectHasPropertyForKey(ctx, object, key, reinterpret_cast<const void**>(exception));
}

bool StringValueEquals(JSContextRef ctx, JSValueRef value, const char* expected) {
    if (!JSValueIsString(ctx, value)) return false;
    ScopedJsString copy(JSValueToStringCopy(ctx, value, nullptr));
    return copy.ref && JSStringIsEqualToUTF8CString(copy, expected);
}

bool GlobalCheck(JSContextRef ctx, const char* name, JSValueRef value, const char* predicate) {
    ScopedJsString property(name);
    JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), property, value, kJSPropertyAttributeNone, nullptr);
    return JSValueToBoolean(ctx, Eval(ctx, predicate));
}

void TestMakeString(JSGlobalContextRef ctx) {
    ScopedJsString text("h\xC3\xA9llo");
    JSValueRef value = MakeString(ctx, text);
    Require(value != nullptr);
    Require(StringValueEquals(ctx, value, "h\xC3\xA9llo"));
    Require(GlobalCheck(ctx, "made", value, "typeof made === 'string' && made.length === 5 && made.charCodeAt(1) === 0xe9"));

    ScopedJsString empty("");
    JSValueRef emptyValue = MakeString(ctx, empty);
    Require(StringValueEquals(ctx, emptyValue, ""));

    JSValueRef nullValue = MakeString(ctx, nullptr);
    Require(nullValue != nullptr);
    Require(StringValueEquals(ctx, nullValue, ""));
    Require(GlobalCheck(ctx, "fromNull", nullValue, "fromNull === ''"));

    JSValueRef again = MakeString(ctx, text);
    Require(JSValueIsStrictEqual(ctx, value, again));

    Require(JSValueMakeString(nullptr, text) == nullptr);
}

void TestHasPropertyForKey(JSGlobalContextRef ctx) {
    JSObjectRef object = JSValueToObject(ctx, Eval(ctx, "globalThis.sym = Symbol('k'); ({ a: 1, 3: 'x', [sym]: 2 })"), nullptr);
    Require(object != nullptr);

    ScopedJsString a("a");
    ScopedJsString b("b");
    ScopedJsString inherited("toString");
    Require(HasPropertyForKey(ctx, object, MakeString(ctx, a), nullptr));
    Require(!HasPropertyForKey(ctx, object, MakeString(ctx, b), nullptr));
    Require(HasPropertyForKey(ctx, object, MakeString(ctx, inherited), nullptr));
    Require(HasPropertyForKey(ctx, object, JSValueMakeNumber(ctx, 3), nullptr));
    Require(!HasPropertyForKey(ctx, object, JSValueMakeNumber(ctx, 4), nullptr));
    Require(HasPropertyForKey(ctx, object, Eval(ctx, "sym"), nullptr));
    Require(!HasPropertyForKey(ctx, object, Eval(ctx, "Symbol('k')"), nullptr));

    JSObjectRef proxy = JSValueToObject(ctx, Eval(ctx, "new Proxy({}, { has(target, key) { return key === 'trap'; } })"), nullptr);
    ScopedJsString trap("trap");
    Require(HasPropertyForKey(ctx, proxy, MakeString(ctx, trap), nullptr));
    Require(!HasPropertyForKey(ctx, proxy, MakeString(ctx, a), nullptr));

    JSValueRef throwingKey = Eval(ctx, "({ toString() { throw new Error('boom'); } })");
    JSValueRef exception = nullptr;
    Require(!HasPropertyForKey(ctx, object, throwingKey, &exception));
    Require(exception != nullptr);
    Require(GlobalCheck(ctx, "caught", exception, "caught instanceof Error && caught.message === 'boom'"));
    Require(!HasPropertyForKey(ctx, object, throwingKey, nullptr));

    JSObjectRef throwingProxy = JSValueToObject(ctx, Eval(ctx, "new Proxy({}, { has() { throw new TypeError('trap'); } })"), nullptr);
    JSValueRef trapException = nullptr;
    Require(!HasPropertyForKey(ctx, throwingProxy, MakeString(ctx, a), &trapException));
    Require(trapException != nullptr);
    Require(GlobalCheck(ctx, "trapCaught", trapException, "trapCaught instanceof TypeError"));

    exception = nullptr;
    Require(HasPropertyForKey(ctx, object, MakeString(ctx, a), &exception));
    Require(exception == nullptr);

    Require(!JSObjectHasPropertyForKey(nullptr, object, MakeString(ctx, a), nullptr));
    Require(!HasPropertyForKey(ctx, nullptr, MakeString(ctx, a), nullptr));
    Require(!HasPropertyForKey(ctx, object, nullptr, nullptr));
}

}

int main() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    Require(ctx != nullptr);
    TestMakeString(ctx);
    TestHasPropertyForKey(ctx);
    JSGlobalContextRelease(ctx);
    return 0;
}
