#include "../src/Jsc.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceJsc/include/Api.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>

using namespace NKWebKit;

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JscScriptThis check failed at line %u\n", location.line());
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

JSValueRef Eval(JSContextRef ctx, const char* code) {
    ScopedJsString script(code);
    JSValueRef exception = nullptr;
    JSValueRef value = JSEvaluateScript(ctx, script, nullptr, nullptr, 1, &exception);
    Require(value != nullptr && exception == nullptr);
    return value;
}

struct Fixture {
    JSContextGroupRef group;
    JSGlobalContextRef ctx;

    Fixture() : group(JSContextGroupCreate()), ctx(JSGlobalContextCreateInGroup(group, nullptr)) {
        Require(group != nullptr && ctx != nullptr);
    }
    Fixture(const Fixture&) = delete;
    ~Fixture() {
        JSGlobalContextRelease(ctx);
        JSContextGroupRelease(group);
    }
};

bool Holds(const Fixture& f, const char* code, JSValueRef thisValue,
    std::source_location location = std::source_location::current()) {
    ScopedJsString source(code);
    JSScriptRef script = JSScriptCreateFromString(f.group, nullptr, 1, source, nullptr, nullptr);
    Require(script != nullptr, location);
    JSValueRef exception = nullptr;
    JSValueRef value = JSScriptEvaluate(f.ctx, script, thisValue, &exception);
    Require(value != nullptr, location);
    Require(exception == nullptr, location);
    const bool result = JSValueToBoolean(f.ctx, value);
    JSScriptRelease(script);
    return result;
}

void TestGlobalObjectAsThis() {
    Fixture f;
    const JSValueRef globalLike[] = {
        nullptr, JSValueMakeUndefined(f.ctx), JSValueMakeNull(f.ctx), JSContextGetGlobalObject(f.ctx)};
    for (JSValueRef thisValue : globalLike) {
        Require(Holds(f, "this === globalThis", thisValue));
        Require(Holds(f, "typeof this === 'object'", thisValue));
        Require(Holds(f, "this.Array === Array", thisValue));
    }
}

void TestPrimitiveBecomesAnObject() {
    Fixture f;
    const JSValueRef number = JSValueMakeNumber(f.ctx, 42.0);
    Require(Holds(f, "typeof this === 'object'", number));
    Require(Holds(f, "this !== globalThis", number));
    Require(Holds(f, "this instanceof Number", number));
    Require(Holds(f, "this.valueOf() === 42", number));
    Require(Holds(f, "Object.prototype.toString.call(this) === '[object Number]'", number));

    const JSValueRef zero = JSValueMakeNumber(f.ctx, 0.0);
    Require(Holds(f, "typeof this === 'object' && this.valueOf() === 0", zero));

    const JSValueRef text = Eval(f.ctx, "'abc'");
    Require(Holds(f, "typeof this === 'object'", text));
    Require(Holds(f, "this instanceof String", text));
    Require(Holds(f, "this.valueOf() === 'abc' && this.length === 3", text));

    const JSValueRef empty = Eval(f.ctx, "''");
    Require(Holds(f, "typeof this === 'object' && this.valueOf() === ''", empty));

    const JSValueRef yes = JSValueMakeBoolean(f.ctx, true);
    Require(Holds(f, "typeof this === 'object'", yes));
    Require(Holds(f, "this instanceof Boolean && this.valueOf() === true", yes));

    const JSValueRef no = JSValueMakeBoolean(f.ctx, false);
    Require(Holds(f, "typeof this === 'object'", no));
    Require(Holds(f, "this instanceof Boolean && this.valueOf() === false", no));

    const JSValueRef symbol = Eval(f.ctx, "Symbol.iterator");
    Require(Holds(f, "typeof this === 'object' && this.valueOf() === Symbol.iterator", symbol));
}

void TestObjectStaysItself() {
    Fixture f;
    const JSValueRef object = Eval(f.ctx, "globalThis.kept = { x: 7 }; kept");
    Require(Holds(f, "this === kept", object));
    Require(Holds(f, "this.x === 7", object));
    Require(Holds(f, "this !== globalThis", object));

    const JSValueRef array = Eval(f.ctx, "globalThis.list = [1, 2, 3]; list");
    Require(Holds(f, "this === list && this.length === 3", array));

    const JSValueRef function = Eval(f.ctx, "globalThis.fn = function () {}; fn");
    Require(Holds(f, "this === fn && typeof this === 'function'", function));
}

void TestResultAndException() {
    Fixture f;
    ScopedJsString valueSource("this.valueOf() + 1");
    JSScriptRef valueScript = JSScriptCreateFromString(f.group, nullptr, 1, valueSource, nullptr, nullptr);
    Require(valueScript != nullptr);
    JSValueRef exception = nullptr;
    JSValueRef value = JSScriptEvaluate(f.ctx, valueScript, JSValueMakeNumber(f.ctx, 41.0), &exception);
    Require(value != nullptr && exception == nullptr);
    Require(JSValueToNumber(f.ctx, value, nullptr) == 42.0);
    JSScriptRelease(valueScript);

    ScopedJsString throwingSource("throw this.valueOf()");
    JSScriptRef throwingScript = JSScriptCreateFromString(f.group, nullptr, 1, throwingSource, nullptr, nullptr);
    Require(throwingScript != nullptr);
    exception = nullptr;
    Require(JSScriptEvaluate(f.ctx, throwingScript, JSValueMakeNumber(f.ctx, 5.0), &exception) == nullptr);
    Require(exception != nullptr);
    Require(JSValueToNumber(f.ctx, exception, nullptr) == 5.0);
    exception = nullptr;
    Require(JSScriptEvaluate(f.ctx, throwingScript, JSValueMakeUndefined(f.ctx), &exception) == nullptr);
    Require(exception != nullptr);
    JSScriptRelease(throwingScript);
}

}  // namespace

int main() {
    TestGlobalObjectAsThis();
    TestPrimitiveBecomesAnObject();
    TestObjectStaysItself();
    TestResultAndException();
    std::puts("libSceNKWebKit JSScriptEvaluate this tests passed");
    return 0;
}
