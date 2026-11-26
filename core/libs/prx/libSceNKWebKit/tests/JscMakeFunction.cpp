#include "../src/Jsc.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceJsc/include/Api.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <vector>

using namespace NKWebKit;

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JSC make function check failed at line %u\n", location.line());
    std::abort();
}

struct ScopedJsString {
    JSStringRef ref = nullptr;
    explicit ScopedJsString(const char* utf8) : ref(utf8 ? JSStringCreateWithUTF8CString_nid_postfix(utf8) : nullptr) {}
    ScopedJsString(const ScopedJsString&) = delete;
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

JSObjectRef Make(JSContextRef ctx, const char* name, std::vector<const char*> parameters, const char* body, JSValueRef* exception) {
    ScopedJsString nameString(name);
    ScopedJsString bodyString(body);
    std::vector<JSStringRef> names;
    for (const char* parameter : parameters) names.push_back(JSStringCreateWithUTF8CString_nid_postfix(parameter));
    JSObjectRef function = JSObjectMakeFunction(ctx, nameString, static_cast<unsigned>(names.size()), names.empty() ? nullptr : names.data(), bodyString, nullptr, 1, exception);
    for (JSStringRef parameter : names) JSStringRelease_nid_postfix(parameter);
    return function;
}

void RequireMade(JSContextRef ctx, const char* name, std::vector<const char*> parameters, const char* body, const char* check, std::source_location location = std::source_location::current()) {
    JSValueRef exception = nullptr;
    JSObjectRef function = Make(ctx, name, std::move(parameters), body, &exception);
    Require(function != nullptr && exception == nullptr, location);
    SetGlobal(ctx, "made", function);
    Require(EvalBool(ctx, check, location), location);
}

void RequireRefused(JSContextRef ctx, const char* name, std::vector<const char*> parameters, const char* body, std::source_location location = std::source_location::current()) {
    JSValueRef exception = nullptr;
    Require(Make(ctx, name, std::move(parameters), body, &exception) == nullptr, location);
    Require(exception != nullptr, location);
    SetGlobal(ctx, "refusal", exception);
    Require(EvalBool(ctx, "refusal instanceof SyntaxError && typeof ran === 'undefined'", location), location);
}

void TestFunctionsThatAreMade(JSContextRef ctx) {
    RequireMade(ctx, nullptr, {}, "return 7;", "made() === 7 && made.name === 'anonymous' && made.length === 0");
    RequireMade(ctx, "adder", {"a", "b"}, "return a + b;", "made(1, 2) === 3 && made.name === 'adder' && made.length === 2");
    RequireMade(ctx, nullptr, {"a", "b"}, "return a + b;", "String(made) === 'function anonymous(a, b) {\\nreturn a + b;\\n}'");
    RequireMade(ctx, nullptr, {}, "", "made() === undefined");
    RequireMade(ctx, nullptr, {}, "return 1", "made() === 1");
    RequireMade(ctx, nullptr, {}, "return 2; // trailing comment", "made() === 2");
    RequireMade(ctx, nullptr, {}, "return '}), ran = 1, (function () {';", "made() === '}), ran = 1, (function () {'");
    RequireMade(ctx, nullptr, {}, "/* } */ return `}${1}{` + /[}]/.source;", "made() === '}1{[}]'");
    RequireMade(ctx, nullptr, {"x"}, "if (x) { return function inner() { return { y: x }; }; } return null;", "made(4)().y === 4 && made(0) === null");
    RequireMade(ctx, nullptr, {"a = 1", "{ b } = { b: 2 }", "...rest"}, "return a + b + rest.length;", "made() === 3 && made(5, { b: 1 }, 0, 0) === 8");
    RequireMade(ctx, nullptr, {}, "var scopeProbe = 3; let scopeProbe0 = 4; return scopeProbe + scopeProbe0;", "made() === 7");
    RequireMade(ctx, nullptr, {"scopeProbe"}, "return scopeProbe;", "made(9) === 9");
    RequireMade(ctx, nullptr, {}, "var let = 5; let", "made() === undefined");
    RequireMade(ctx, nullptr, {}, "var x = 1\nx\n++\nx", "made() === undefined");
    RequireMade(ctx, nullptr, {}, "do { } while (false)", "made() === undefined");
    RequireMade(ctx, nullptr, {}, "'use strict'; return this;", "made() === undefined");
    RequireMade(ctx, nullptr, {}, "label: for (;;) { break label; } return 6;", "made() === 6");
}

void TestTextThatIsNotOneFunction(JSContextRef ctx) {
    RequireRefused(ctx, nullptr, {}, "}), ran = 1, (function () {");
    RequireRefused(ctx, nullptr, {}, "return 1; }), (ran = 1), (function () { return 2;");
    RequireRefused(ctx, nullptr, {}, "} + (ran = 1) + {");
    RequireRefused(ctx, nullptr, {}, "} ran = 1; function later() {");
    RequireRefused(ctx, nullptr, {"a) {}, (ran = 1), function (b"}, "return b;");
    RequireRefused(ctx, nullptr, {"a", "b) { return 1; }, function (c"}, "return c;");
    RequireRefused(ctx, "first() {}, (ran = 1), function second", {}, "return 1;");
    RequireRefused(ctx, "", {}, "return 1;");
    RequireRefused(ctx, "class", {}, "return 1;");
    RequireRefused(ctx, nullptr, {"a /*"}, "*/) {");
    RequireRefused(ctx, nullptr, {"a = '"}, "') {");
    RequireRefused(ctx, nullptr, {"a", "b /*"}, "*/, c) { return c;");
    RequireRefused(ctx, nullptr, {"a = `x"}, "` ) { ran = 1; //`");
    RequireRefused(ctx, nullptr, {"a //"}, ") { ran = 1;");
    RequireRefused(ctx, nullptr, {}, "}), ran = 1, (function () {");
    Require(EvalBool(ctx, "refusal.message === 'Parser error'"));
    RequireRefused(ctx, nullptr, {"a /*"}, "*/) {");
    Require(EvalBool(ctx, "refusal.message === 'Parameters should match arguments offered as parameters in Function constructor.'"));
    RequireRefused(ctx, nullptr, {}, "rreturn Array;");
    RequireRefused(ctx, nullptr, {"a", "a"}, "'use strict';");
    RequireRefused(ctx, nullptr, {}, "if (true) {");
    Require(EvalBool(ctx, "typeof ran === 'undefined' && typeof later === 'undefined' && typeof first === 'undefined' && typeof second === 'undefined'"));
}

}  // namespace

int main() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    TestFunctionsThatAreMade(ctx);
    TestTextThatIsNotOneFunction(ctx);
    TestFunctionsThatAreMade(ctx);
    JSGlobalContextRelease(ctx);
    std::puts("libSceNKWebKit JS make function tests passed");
    return 0;
}
