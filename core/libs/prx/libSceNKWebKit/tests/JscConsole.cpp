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
    std::fprintf(stderr, "JSC console check failed at line %u\n", location.line());
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

const char* const AllNames =
    "debug,error,log,info,warn,clear,dir,dirxml,table,trace,assert,count,countReset,profile,profileEnd,"
    "time,timeLog,timeEnd,timeStamp,takeHeapSnapshot,group,groupCollapsed,groupEnd,record,recordEnd,screenshot";

const char* const ConvertingNames = "count,countReset,profile,profileEnd,time,timeLog,timeEnd,takeHeapSnapshot";

void DefineNameLists(JSContextRef ctx) {
    char code[512];
    std::snprintf(code, sizeof(code), "var allNames = '%s'.split(','); var convertingNames = '%s'.split(','); true", AllNames, ConvertingNames);
    Require(EvalBool(ctx, code));
}

void TestTheObject(JSContextRef ctx) {
    Require(EvalBool(ctx, "typeof console === 'object' && String(console) === '[object console]' && Object.prototype.toString.call(console) === '[object console]'"));
    Require(EvalBool(ctx, "var d = Object.getOwnPropertyDescriptor(this, 'console'); d.value === console && d.writable === true && d.enumerable === false && d.configurable === true"));
    Require(EvalBool(ctx, "var listed = false; for (var key in this) if (key === 'console') listed = true; !listed && Object.keys(this).indexOf('console') < 0"));
    Require(EvalBool(ctx, "var proto = Object.getPrototypeOf(console);"
                          "proto !== Object.prototype && Object.getPrototypeOf(proto) === Object.prototype && Reflect.ownKeys(proto).length === 0"));
    Require(EvalBool(ctx, "Object.keys(console).join() === allNames.join()"));
    Require(EvalBool(ctx, "var own = Reflect.ownKeys(console); own.length === 27 && own[26] === Symbol.toStringTag"));
    Require(EvalBool(ctx, "var tag = Object.getOwnPropertyDescriptor(console, Symbol.toStringTag);"
                          "tag.value === 'console' && tag.writable === false && tag.enumerable === false && tag.configurable === true"));
    ScopedJsString name("console");
    JSObjectRef global = JSContextGetGlobalObject(ctx);
    Require(JSObjectHasProperty(ctx, global, name));
    JSValueRef exception = nullptr;
    JSValueRef console = JSObjectGetProperty(ctx, global, name, &exception);
    Require(exception == nullptr && console != nullptr && JSValueIsObject(ctx, console));
}

void TestTheFunctions(JSContextRef ctx) {
    Require(EvalBool(ctx, "allNames.every(function (name) {"
                          "  var d = Object.getOwnPropertyDescriptor(console, name), f = d.value;"
                          "  return typeof f === 'function' && f.length === 0 && f.name === name && d.writable && d.enumerable && d.configurable;"
                          "})"));
    Require(EvalBool(ctx, "allNames.every(function (name) {"
                          "  return console[name]() === undefined && console[name](1, 'two', {}) === undefined && console[name](null) === undefined && console[name](undefined, 1) === undefined;"
                          "})"));
    Require(EvalBool(ctx, "allNames.every(function (name) { var f = console[name]; return f(1) === undefined && f.call(null, 'label') === undefined && f.apply({}, []) === undefined; })"));
    Require(EvalBool(ctx, "allNames.every(function (name) { try { new console[name](); } catch (error) { return error instanceof TypeError; } return false; })"));
    Require(EvalBool(ctx, "allNames.every(function (name) { return !Object.prototype.hasOwnProperty.call(console[name], 'prototype'); })"));
}

void TestWhichFunctionsConvertTheirFirstArgument(JSContextRef ctx) {
    Require(EvalBool(ctx, "allNames.every(function (name) {"
                          "  var calls = 0, counted = { toString: function () { calls++; return 'label'; } };"
                          "  console[name](counted);"
                          "  return calls === (convertingNames.indexOf(name) >= 0 ? 1 : 0);"
                          "})"));
    Require(EvalBool(ctx, "allNames.every(function (name) {"
                          "  var calls = 0, counted = { toString: function () { calls++; return 'label'; } };"
                          "  console[name]('first', counted, counted);"
                          "  return calls === 0;"
                          "})"));
    Require(EvalBool(ctx, "allNames.every(function (name) {"
                          "  var thrown = null, thrower = { toString: function () { throw 'from toString'; } };"
                          "  try { console[name](thrower); } catch (error) { thrown = error; }"
                          "  return thrown === (convertingNames.indexOf(name) >= 0 ? 'from toString' : null);"
                          "})"));
    Require(EvalBool(ctx, "allNames.every(function (name) {"
                          "  var raised = false;"
                          "  try { console[name](Symbol('label')); } catch (error) { raised = error instanceof TypeError; }"
                          "  return raised === (convertingNames.indexOf(name) >= 0);"
                          "})"));
    Require(EvalBool(ctx, "allNames.every(function (name) {"
                          "  var hints = [], primitive = {}; primitive[Symbol.toPrimitive] = function (hint) { hints.push(hint); return 1; };"
                          "  console[name](primitive);"
                          "  return hints.join() === (convertingNames.indexOf(name) >= 0 ? 'string' : '');"
                          "})"));
    Require(EvalBool(ctx, "console.assert({ valueOf: function () { throw 1; }, toString: function () { throw 2; } }) === undefined"));
}

void TestTheGlobalPropertyCanBeReplaced(JSContextRef ctx) {
    Require(EvalBool(ctx, "var original = console; console = 5; console === 5 && original.log() === undefined"));
    Require(EvalBool(ctx, "delete this.console && typeof console === 'undefined' && !('console' in this)"));
}

void TestEachContextHasItsOwn() {
    JSContextGroupRef group = JSContextGroupCreate();
    JSGlobalContextRef first = JSGlobalContextCreateInGroup(group, nullptr);
    JSGlobalContextRef second = JSGlobalContextCreateInGroup(group, nullptr);
    JSValueRef firstConsole = Eval(first, "console");
    JSValueRef secondConsole = Eval(second, "console");
    Require(JSValueIsObject(first, firstConsole) && JSValueIsObject(second, secondConsole));
    Require(!JSValueIsStrictEqual(first, firstConsole, secondConsole));
    Require(EvalBool(first, "console.marker = 1; console.marker === 1"));
    Require(EvalBool(second, "console.marker === undefined"));
    JSGlobalContextRelease(second);
    JSGlobalContextRelease(first);
    JSContextGroupRelease(group);
}

void TestAGlobalObjectWithAClass() {
    JSClassDefinition definition = kJSClassDefinitionEmpty;
    JSClassRef jsClass = JSClassCreate(&definition);
    JSGlobalContextRef ctx = JSGlobalContextCreate(jsClass);
    DefineNameLists(ctx);
    TestTheObject(ctx);
    TestTheFunctions(ctx);
    JSGlobalContextRelease(ctx);
    JSClassRelease(jsClass);
}

}  // namespace

int main() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    DefineNameLists(ctx);
    TestTheObject(ctx);
    TestTheFunctions(ctx);
    TestWhichFunctionsConvertTheirFirstArgument(ctx);
    TestTheGlobalPropertyCanBeReplaced(ctx);
    JSGlobalContextRelease(ctx);
    TestEachContextHasItsOwn();
    TestAGlobalObjectWithAClass();
    std::puts("libSceNKWebKit JS console tests passed");
    return 0;
}
