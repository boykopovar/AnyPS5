#include "../src/Jsc.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceJsc/include/Api.hpp"
#include "prx/libSceJsc/include/ApiBridgeExports.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>

using namespace NKWebKit;

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JSC error info check failed at line %u\n", location.line());
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

JSValueRef Run(JSContextRef ctx, const char* code, const char* url, int line, JSValueRef* exception) {
    ScopedJsString script(code);
    ScopedJsString sourceUrl(url);
    return JSEvaluateScript(ctx, script, nullptr, sourceUrl, line, exception);
}

bool Check(JSContextRef ctx, const char* code, const char* url = "file:///dir/app.js", int line = 1, std::source_location location = std::source_location::current()) {
    JSValueRef exception = nullptr;
    JSValueRef value = Run(ctx, code, url, line, &exception);
    Require(exception == nullptr && value != nullptr, location);
    return JSValueToBoolean(ctx, value);
}

void SetGlobal(JSContextRef ctx, const char* name, JSValueRef value) {
    ScopedJsString key(name);
    JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), key, value, kJSPropertyAttributeNone, nullptr);
}

JSValueRef MakeText(JSContextRef ctx, const char* utf8) {
    ScopedJsString text(utf8);
    return static_cast<JSValueRef>(JSValueMakeString(ctx, text));
}

JSValueRef APS5_VABI MakeErrorInCallback(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, std::size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) {
    (void)function;
    (void)thisObject;
    (void)argumentCount;
    (void)arguments;
    (void)exception;
    JSValueRef message = MakeText(ctx, "made in C");
    return JSObjectMakeError(ctx, 1, &message, nullptr);
}

const char* const Helpers =
    "function hidden(object, name) { var d = Object.getOwnPropertyDescriptor(object, name);"
    "  return !!d && 'value' in d && d.writable === true && d.enumerable === false && d.configurable === true; }"
    "function shown(object, name) { var d = Object.getOwnPropertyDescriptor(object, name);"
    "  return !!d && 'value' in d && d.writable === true && d.enumerable === true && d.configurable === true; }"
    "function names(object) { return Object.getOwnPropertyNames(object).join(); } true";

void TestAnErrorMadeInAFunction(JSContextRef ctx) {
    Require(Check(ctx, "function make() {\n  return new Error('boom');\n}\nvar made = make();\ntrue"));
    Require(Check(ctx, "names(made) === 'message,line,column,sourceURL,stack'"));
    Require(Check(ctx, "made.line === 2 && made.sourceURL === 'file:///dir/app.js' && typeof made.column === 'number' && made.column > 0"));
    Require(Check(ctx, "hidden(made, 'line') && hidden(made, 'column') && hidden(made, 'sourceURL') && hidden(made, 'stack')"));
    Require(Check(ctx, "Object.keys(made).length === 0 && JSON.stringify(made) === '{}'"));
    Require(Check(ctx, "var parts = /^make@file:\\/\\/\\/dir\\/app\\.js:2:(\\d+)\\nglobal code@file:\\/\\/\\/dir\\/app\\.js:4:\\d+$/.exec(made.stack); parts !== null && Number(parts[1]) === made.column"));
    Require(Check(ctx, "names(new Error()) === 'line,column,sourceURL,stack'"));
    Require(Check(ctx, "made.stack = 'replaced'; made.stack === 'replaced' && delete made.line && made.line === undefined"));
}

void TestStartingLineAndMissingUrl(JSContextRef ctx) {
    Require(Check(ctx, "\nvar later = new Error('x'); later.line === 11 && /^global code@file:\\/\\/\\/dir\\/other\\.js:11:\\d+$/.test(later.stack) && later.sourceURL === 'file:///dir/other.js'", "file:///dir/other.js", 10));
    Require(Check(ctx, "function inner() { return new Error('x'); }\nvar bare = inner(); names(bare) === 'message,line,column,stack' && bare.line === 1 && bare.stack === 'inner@\\nglobal code@'", nullptr, 1));
    Require(Check(ctx, "var plainName = new Error('x'); plainName.sourceURL === 'test script' && /^global code@test script:1:\\d+$/.test(plainName.stack)", "test script", 1));
}

void TestErrorsOfTheEngineAndNativeFrames(JSContextRef ctx) {
    Require(Check(ctx, "var typeError;\ntry {\n  null.x;\n} catch (error) { typeError = error; }\ntypeError instanceof TypeError && typeError.line === 3 && typeError.sourceURL === 'file:///dir/app.js' && /^global code@file:\\/\\/\\/dir\\/app\\.js:3:\\d+$/.test(typeError.stack)"));
    Require(Check(ctx, "var fromCallback;\n[1].forEach(function each() {\n  fromCallback = new Error('x');\n});\n"
                       "fromCallback.line === 3 && /^each@file:\\/\\/\\/dir\\/app\\.js:3:\\d+\\nforEach@\\[native code\\]\\nglobal code@file:\\/\\/\\/dir\\/app\\.js:2:\\d+$/.test(fromCallback.stack)"));
    Require(Check(ctx, "var rangeError;\ntry {\n  new Array(-1);\n} catch (error) { rangeError = error; }\n"
                       "rangeError instanceof RangeError && rangeError.line === 3 && rangeError.sourceURL === 'file:///dir/app.js' && /^Array@\\[native code\\]\\nglobal code@file:\\/\\/\\/dir\\/app\\.js:3:\\d+$/.test(rangeError.stack)"));
    Require(Check(ctx, "var anonymous = (function () { return new Error('x'); })(); /^@file:\\/\\/\\/dir\\/app\\.js:1:\\d+\\nglobal code@file:\\/\\/\\/dir\\/app\\.js:1:\\d+$/.test(anonymous.stack)"));
    Require(Check(ctx, "class Custom extends Error {\n  constructor(message) {\n    super(message);\n  }\n}\nvar custom = new Custom('c');\n"
                       "custom.line === 3 && hidden(custom, 'stack') && /^Custom@file:\\/\\/\\/dir\\/app\\.js:3:\\d+\\nglobal code@file:\\/\\/\\/dir\\/app\\.js:6:\\d+$/.test(custom.stack)"));
    Require(Check(ctx, "var thrown; try { throw 5; } catch (value) { thrown = value; } thrown === 5"));
}

void TestEvaluatedText(JSContextRef ctx) {
    Require(Check(ctx, "var inEval = eval('\\n\\nnew Error(\"e\")');\n"
                       "inEval.line === 3 && !('sourceURL' in inEval) && /^eval code@\\nglobal code@file:\\/\\/\\/dir\\/app\\.js:1:\\d+$/.test(inEval.stack)"));
    Require(Check(ctx, "var badEval;\nfunction runEval() {\n  try { eval('\\n\\n\\nrreturn 1;'); } catch (error) { badEval = error; }\n}\nrunEval();\n"
                       "badEval instanceof SyntaxError && badEval.line === 4 && shown(badEval, 'line') && hidden(badEval, 'column') && badEval.sourceURL === 'file:///dir/app.js' && hidden(badEval, 'sourceURL')"
                       " && /^runEval@file:\\/\\/\\/dir\\/app\\.js:3:\\d+\\nglobal code@file:\\/\\/\\/dir\\/app\\.js:5:\\d+$/.test(badEval.stack)"));
}

void TestSyntaxErrorsOfTheApi(JSContextRef ctx) {
    JSValueRef exception = nullptr;
    Require(Run(ctx, "\nrreturn Array;", "file:///foo/bar.js", 1, &exception) == nullptr && exception != nullptr);
    SetGlobal(ctx, "apiSyntax", exception);
    Require(Check(ctx, "apiSyntax instanceof SyntaxError && names(apiSyntax) === 'message,line,sourceURL' && apiSyntax.line === 2 && apiSyntax.sourceURL === 'file:///foo/bar.js'"));
    Require(Check(ctx, "shown(apiSyntax, 'line') && shown(apiSyntax, 'sourceURL') && apiSyntax.stack === undefined && apiSyntax.column === undefined"));
    ScopedJsString lineKey("line");
    ScopedJsString urlKey("sourceURL");
    JSObjectRef thrown = JSValueToObject(ctx, exception, nullptr);
    Require(JSValueToNumber(ctx, JSObjectGetProperty(ctx, thrown, lineKey, nullptr), nullptr) == 2.0);
    JSStringRef url = JSValueToStringCopy(ctx, JSObjectGetProperty(ctx, thrown, urlKey, nullptr), nullptr);
    Require(url != nullptr && JSStringIsEqualToUTF8CString(url, "file:///foo/bar.js"));
    JSStringRelease_nid_postfix(url);

    exception = nullptr;
    Require(Run(ctx, "var a = ;", nullptr, 7, &exception) == nullptr && exception != nullptr);
    SetGlobal(ctx, "noUrlSyntax", exception);
    Require(Check(ctx, "names(noUrlSyntax) === 'message,line' && noUrlSyntax.line === 7"));

    const struct {
        const char* body;
        int startingLine;
        double expectedLine;
    } bodies[] = {{"rreturn Array;", 1, 2.0}, {"rreturn Array;", -42, 2.0}, {"// Line one.\nrreturn Array;", 1, 3.0}, {"rreturn Array;", 20, 21.0}};
    for (const auto& entry : bodies) {
        ScopedJsString body(entry.body);
        exception = nullptr;
        Require(JSObjectMakeFunction(ctx, nullptr, 0, nullptr, body, nullptr, entry.startingLine, &exception) == nullptr);
        Require(exception != nullptr && JSValueIsObject(ctx, exception));
        Require(JSValueToNumber(ctx, JSObjectGetProperty(ctx, JSValueToObject(ctx, exception, nullptr), lineKey, nullptr), nullptr) == entry.expectedLine);
    }

    ScopedJsString broken("\n\nvar b = ;");
    ScopedJsString checkedUrl("file:///checked.js");
    exception = nullptr;
    Require(!JSCheckScriptSyntax(ctx, broken, checkedUrl, 1, &exception) && exception != nullptr);
    SetGlobal(ctx, "checkedSyntax", exception);
    Require(Check(ctx, "checkedSyntax.line === 3 && checkedSyntax.sourceURL === 'file:///checked.js'"));
}

void TestErrorsMadeThroughTheApi(JSContextRef ctx) {
    JSValueRef message = MakeText(ctx, "outside");
    JSObjectRef outside = JSObjectMakeError(ctx, 1, &message, nullptr);
    Require(outside != nullptr);
    SetGlobal(ctx, "madeOutside", outside);
    Require(Check(ctx, "names(madeOutside) === 'message' && madeOutside.stack === undefined && madeOutside.line === undefined"));

    ScopedJsString name("makeError");
    SetGlobal(ctx, "makeError", JSObjectMakeFunctionWithCallback(ctx, name, &MakeErrorInCallback));
    Require(Check(ctx, "\nvar madeInside = makeError();\nmadeInside.message === 'made in C' && madeInside.line === 2 && madeInside.sourceURL === 'file:///dir/app.js'"
                       " && /global code@file:\\/\\/\\/dir\\/app\\.js:2:\\d+$/.test(madeInside.stack)"));

    JSValueRef exception = nullptr;
    Require(Run(ctx, "\n\nfunction fail() {\n  throw new Error('thrown');\n}\nfail();", "file:///dir/throw.js", 1, &exception) == nullptr && exception != nullptr);
    JSObjectRef thrown = JSValueToObject(ctx, exception, nullptr);
    ScopedJsString lineKey("line");
    ScopedJsString urlKey("sourceURL");
    ScopedJsString stackKey("stack");
    Require(JSValueToNumber(ctx, JSObjectGetProperty(ctx, thrown, lineKey, nullptr), nullptr) == 4.0);
    JSStringRef url = JSValueToStringCopy(ctx, JSObjectGetProperty(ctx, thrown, urlKey, nullptr), nullptr);
    Require(url != nullptr && JSStringIsEqualToUTF8CString(url, "file:///dir/throw.js"));
    JSStringRelease_nid_postfix(url);
    Require(JSValueIsString(ctx, JSObjectGetProperty(ctx, thrown, stackKey, nullptr)));
}

void TestTheHookIsNotVisible(JSContextRef ctx) {
    Require(Check(ctx, "Error.stackTraceLimit === 100"));
    Require(Check(ctx, "function nest(depth) { return depth ? nest(depth - 1) : new Error('deep'); }\nvar lines = nest(30).stack.split('\\n'); lines.length === 32 && lines[30].indexOf('nest@') === 0 && lines[31].indexOf('global code@') === 0"));
    Require(Check(ctx, "typeof Error.prepareStackTrace === 'undefined' && !('prepareStackTrace' in Error)"));
    Require(Check(ctx, "Error.prepareStackTrace = function () { return 'custom'; };\nvar afterAssign = new Error('x');\n"
                       "delete Error.prepareStackTrace && afterAssign.line === 2 && /^global code@/.test(afterAssign.stack)"));
}

void TestTheLineOfAScriptThatDoesNotCompile() {
    JSContextGroupRef group = JSContextGroupCreate();
    ScopedJsString url("file:///dir/script.js");
    ScopedJsString source("\n\nvar broken = ;");
    JSStringRef message = nullptr;
    int line = 0;
    Require(JSScriptCreateFromString(group, url, 5, source, &message, &line) == nullptr);
    Require(message != nullptr && line == 7);
    JSStringRelease_nid_postfix(message);

    JSGlobalContextRef ctx = JSGlobalContextCreateInGroup(group, nullptr);
    message = nullptr;
    line = 0;
    Require(JSScriptCreateFromString(group, url, 5, source, &message, &line) == nullptr);
    Require(message != nullptr && line == 7);
    JSStringRelease_nid_postfix(message);
    JSGlobalContextRelease(ctx);
    JSContextGroupRelease(group);
}

}  // namespace

int main() {
    TestTheLineOfAScriptThatDoesNotCompile();
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    Require(Check(ctx, Helpers));
    TestAnErrorMadeInAFunction(ctx);
    TestStartingLineAndMissingUrl(ctx);
    TestErrorsOfTheEngineAndNativeFrames(ctx);
    TestEvaluatedText(ctx);
    TestSyntaxErrorsOfTheApi(ctx);
    TestErrorsMadeThroughTheApi(ctx);
    TestTheHookIsNotVisible(ctx);

    JSGlobalContextRef second = JSGlobalContextCreateInGroup(JSContextGetGroup(ctx), nullptr);
    Require(Check(second, "var other = new Error('x'); other.line === 1 && other.sourceURL === 'file:///dir/app.js' && typeof Error.prepareStackTrace === 'undefined'"));
    JSGlobalContextRelease(second);
    JSGlobalContextRelease(ctx);
    std::puts("libSceNKWebKit JS error info tests passed");
    return 0;
}
