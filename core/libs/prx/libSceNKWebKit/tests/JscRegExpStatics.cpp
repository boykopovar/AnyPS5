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
    std::fprintf(stderr, "JSC RegExp statics check failed at line %u\n", location.line());
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

const char* const kSnapshot = "function snapshot() { return [RegExp.lastMatch, RegExp.$1, RegExp.$2, RegExp.$3, RegExp.lastParen, RegExp.leftContext, RegExp.rightContext, RegExp.input].join('|'); }"
                              "function throwsTypeError(f) { try { f(); } catch (e) { return e instanceof TypeError; } return false; } 0";

void TestBeforeAnyMatch(JSContextRef ctx) {
    Require(EvalBool(ctx, "snapshot() === '|||||||' && RegExp.multiline === false && RegExp.$9 === '' && RegExp['$&'] === '' && RegExp['$_'] === ''"));
    Require(EvalBool(ctx, "/zzz/.exec('abc') === null && /zzz/.test('abc') === false && snapshot() === '|||||||'"));
}

void TestWhatAMatchRecords(JSContextRef ctx) {
    Require(EvalBool(ctx, "var m = /(\\d+)-(\\w+)/.exec('ab 12-xy cd'); m[0] === '12-xy' && m.index === 3 && snapshot() === '12-xy|12|xy||xy|ab | cd|ab 12-xy cd'"));
    Require(EvalBool(ctx, "RegExp['$&'] === '12-xy' && RegExp['$+'] === 'xy' && RegExp['$`'] === 'ab ' && RegExp[\"$'\"] === ' cd' && RegExp['$_'] === 'ab 12-xy cd' && RegExp.$4 === '' && RegExp.$9 === ''"));
    Require(EvalBool(ctx, "/zzz/.exec('other') === null && 'other'.match(/zzz/) === null && 'other'.search(/zzz/) === -1 && snapshot() === '12-xy|12|xy||xy|ab | cd|ab 12-xy cd'"));
    Require(EvalBool(ctx, "m[1] = 'changed'; m.input = 'changed'; m.index = 0; snapshot() === '12-xy|12|xy||xy|ab | cd|ab 12-xy cd'"));

    Require(EvalBool(ctx, "/(a)|(b)/.exec('xbz'); snapshot() === 'b||b||b|x|z|xbz'"));
    Require(EvalBool(ctx, "/(a)|(b)/.exec('xaz'); snapshot() === 'a|a|||' + '|x|z|xaz' && RegExp.lastParen === ''"));
    Require(EvalBool(ctx, "/b/.exec('abc'); snapshot() === 'b|||||a|c|abc'"));
    Require(EvalBool(ctx, "/(1)(2)(3)(4)(5)(6)(7)(8)(9)(0)(x)/.exec('1234567890x'); RegExp.$9 === '9' && RegExp.lastParen === 'x' && RegExp.$1 === '1' && RegExp.lastMatch === '1234567890x'"));
    Require(EvalBool(ctx, "/(?<year>\\d{4})/.exec('in 2021!'); RegExp.$1 === '2021' && RegExp.leftContext === 'in ' && RegExp.rightContext === '!'"));
    Require(EvalBool(ctx, "/x/.exec('\\u{1F600}x\\u{1F601}'); RegExp.leftContext === '\\u{1F600}' && RegExp.rightContext === '\\u{1F601}' && RegExp.leftContext.length === 2"));
    Require(EvalBool(ctx, "/$/.exec('end'); snapshot() === '|||||end||end'"));
    Require(EvalBool(ctx, "/(\\d)/.exec(4567); RegExp.input === '4567' && RegExp.$1 === '4' && RegExp.rightContext === '567'"));
}

void TestEveryWayToMatch(JSContextRef ctx) {
    Require(EvalBool(ctx, "/(t)e/.test('a test'); snapshot() === 'te|t|||t|a |st|a test'"));
    Require(EvalBool(ctx, "'one two'.match(/(tw)o/); snapshot() === 'two|tw|||tw|one ||one two'"));
    Require(EvalBool(ctx, "'k1 k2 k3'.match(/k(\\d)/g); RegExp.lastMatch === 'k3' && RegExp.$1 === '3' && RegExp.leftContext === 'k1 k2 ' && RegExp.rightContext === ''"));
    Require(EvalBool(ctx, "'find me'.search(/(m)e/) === 5 && snapshot() === 'me|m|||m|find ||find me'"));
    Require(EvalBool(ctx, "'a-b'.replace(/(-)/, '+') === 'a+b' && snapshot() === '-|-|||-|a|b|a-b'"));
    Require(EvalBool(ctx, "'a1b2'.replace(/(\\d)/g, '') === 'ab' && RegExp.lastMatch === '2' && RegExp.leftContext === 'a1b'"));
    Require(EvalBool(ctx, "'abc'.replace(/(b)/, function () { return RegExp.$1.toUpperCase() + RegExp.leftContext; }) === 'aBac'"));
    Require(EvalBool(ctx, "'x1y2'.replaceAll(/(\\d)/g, '#') === 'x#y#' && RegExp.$1 === '2'"));
    Require(EvalBool(ctx, "'p,q;r'.split(/([,;])/).join('') === 'p,q;r' && RegExp.lastMatch === ';' && RegExp.$1 === ';'"));
    Require(EvalBool(ctx, "var all = Array.from('u7v8'.matchAll(/(\\d)/g)); all.length === 2 && RegExp.$1 === '8' && RegExp.leftContext === 'u7v'"));
    Require(EvalBool(ctx, "'needle here'.match('n(e+)dle'); RegExp.$1 === 'ee' && RegExp.rightContext === ' here'"));
    Require(EvalBool(ctx, "var sticky = /b(c)/y; sticky.lastIndex = 1; sticky.test('abcd') && RegExp.leftContext === 'a' && RegExp.$1 === 'c' && sticky.lastIndex === 3"));
    Require(EvalBool(ctx, "var global = /a/g; global.exec('aa'); global.exec('aa'); global.lastIndex === 2 && RegExp.leftContext === 'a' && global.exec('aa') === null && RegExp.leftContext === 'a'"));

    Require(EvalBool(ctx, "var format = 'yyyy-MM'; if (/(y+)/.test(format)) format = format.replace(RegExp.$1, ('2021').substr(4 - RegExp.$1.length)); format === '2021-MM'"));

    Require(EvalBool(ctx, "/(q)/.exec('q'); var own = /never/; own.exec = function () { return null; }; own.test('never') === false && RegExp.$1 === 'q'"));
    Require(EvalBool(ctx, "var faked = /never/; faked.exec = function () { return ['fake', 'f']; }; faked.test('zzz') === true && RegExp.$1 === 'q' && RegExp.lastMatch === 'q'"));
}

void TestInputAndMultiline(JSContextRef ctx) {
    Require(EvalBool(ctx, "/(b)c/.exec('abcd'); RegExp.input = 5; RegExp.input === '5' && RegExp.$_ === '5' && snapshot() === 'bc|b|||b|a|d|5'"));
    Require(EvalBool(ctx, "RegExp.$_ = 'again'; RegExp.input === 'again' && RegExp.leftContext === 'a' && RegExp.rightContext === 'd' && RegExp.$1 === 'b'"));
    Require(EvalBool(ctx, "/(y)/.exec('xyz'); snapshot() === 'y|y|||y|x|z|xyz'"));
    Require(EvalBool(ctx, "var raised = null; try { RegExp.input = { toString: function () { throw new RangeError('mine'); } }; } catch (e) { raised = e; } raised instanceof RangeError && RegExp.input === 'xyz'"));

    Require(EvalBool(ctx, "RegExp.multiline = 1; RegExp.multiline === true && RegExp['$*'] === true"));
    Require(EvalBool(ctx, "RegExp['$*'] = ''; RegExp.multiline === false && /^b/.test('a\\nb') === false"));
}

void TestThePropertiesThemselves(JSContextRef ctx) {
    Require(EvalBool(ctx, "var names = ['input', '$_', 'multiline', '$*', 'lastMatch', '$&', 'lastParen', '$+', 'leftContext', '$`', 'rightContext', \"$'\", '$1', '$2', '$3', '$4', '$5', '$6', '$7', '$8', '$9'];"
                          "names.every(function (name) { var d = Object.getOwnPropertyDescriptor(RegExp, name); return d && typeof d.get === 'function' && d.enumerable === false && d.configurable === true; })"));
    Require(EvalBool(ctx, "['input', '$_', 'multiline', '$*'].every(function (name) { return typeof Object.getOwnPropertyDescriptor(RegExp, name).set === 'function'; })"));
    Require(EvalBool(ctx, "names.slice(4).every(function (name) { return Object.getOwnPropertyDescriptor(RegExp, name).set === undefined; })"));
    Require(EvalBool(ctx, "Object.keys(RegExp).length === 0 && RegExp.$10 === undefined && RegExp.$0 === undefined"));

    Require(EvalBool(ctx, "/(k)/.exec('k'); RegExp.$1 = 'no'; RegExp.lastMatch = 'no'; RegExp.leftContext = 'no'; RegExp.$1 === 'k' && RegExp.lastMatch === 'k' && RegExp.leftContext === ''"));
    Require(EvalBool(ctx, "throwsTypeError(function () { 'use strict'; RegExp.$1 = 'no'; }) && throwsTypeError(function () { 'use strict'; RegExp.lastParen = 'no'; })"));

    Require(EvalBool(ctx, "names.every(function (name) { var get = Object.getOwnPropertyDescriptor(RegExp, name).get; return throwsTypeError(function () { get.call({}); }) && throwsTypeError(function () { get.call(undefined); }) && throwsTypeError(function () { get.call(/x/); }); })"));
    Require(EvalBool(ctx, "['input', '$*'].every(function (name) { var set = Object.getOwnPropertyDescriptor(RegExp, name).set; return throwsTypeError(function () { set.call({}, 'v'); }); })"));
    Require(EvalBool(ctx, "class Derived extends RegExp {} throwsTypeError(function () { return Derived.$1; }) && throwsTypeError(function () { return Derived.input; }) && throwsTypeError(function () { Derived.input = 'v'; })"));
    Require(EvalBool(ctx, "new Derived('(d)').exec('d'); RegExp.$1 === 'd' && Object.getOwnPropertyDescriptor(RegExp, '$1').get.call(RegExp) === 'd'"));

    Require(EvalBool(ctx, "RegExp.prototype.exec.name === 'exec' && RegExp.prototype.exec.length === 1 && typeof RegExp.prototype.exec === 'function'"));
    Require(EvalBool(ctx, "Function.prototype.toString.call(RegExp.prototype.exec) === Function.prototype.toString.call(RegExp.prototype.test).replace('test', 'exec')"));
    Require(EvalBool(ctx, "var e = Object.getOwnPropertyDescriptor(RegExp.prototype, 'exec'); e.writable === true && e.enumerable === false && e.configurable === true"));
    Require(EvalBool(ctx, "throwsTypeError(function () { RegExp.prototype.exec.call({}, 'x'); }) && throwsTypeError(function () { RegExp.prototype.exec.call('x', 'x'); })"));

    Require(EvalBool(ctx, "var hits = 0; Object.defineProperty(Array.prototype, '1', { set: function () { ++hits; }, configurable: true }); Object.defineProperty(Object.prototype, '1', { set: function () { ++hits; }, configurable: true });"
                          "/(w)/.exec('w'); var clean = hits === 0 && RegExp.$1 === 'w'; delete Array.prototype[1]; delete Object.prototype[1]; clean"));
    Require(EvalBool(ctx, "var realSubstring = String.prototype.substring; String.prototype.substring = function () { return 'replaced'; }; /b/.exec('abc');"
                          "var kept = RegExp.leftContext === 'a' && RegExp.rightContext === 'c'; String.prototype.substring = realSubstring; kept"));
    Require(EvalBool(ctx, "delete RegExp.$9 && RegExp.$9 === undefined && Object.getOwnPropertyDescriptor(RegExp, '$9') === undefined && RegExp.$8 === ''"));
}

void TestEachContextHasItsOwn(JSContextRef ctx) {
    JSGlobalContextRef other = JSGlobalContextCreateInGroup(JSContextGetGroup(ctx), nullptr);
    Eval(other, kSnapshot);
    Require(EvalBool(ctx, "/(mine)/.exec('mine'); RegExp.$1 === 'mine'"));
    Require(EvalBool(other, "snapshot() === '|||||||'"));
    Require(EvalBool(other, "/(theirs)/.exec('theirs'); RegExp.$1 === 'theirs'"));
    Require(EvalBool(ctx, "RegExp.$1 === 'mine'"));

    ScopedJsString name("OtherRegExp");
    JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), name, JSValueToObject(other, Eval(other, "RegExp"), nullptr), kJSPropertyAttributeNone, nullptr);
    Require(EvalBool(ctx, "new OtherRegExp('(cross)').exec('cross'); OtherRegExp.$1 === 'cross' && RegExp.$1 === 'mine'"));
    JSGlobalContextRelease(other);
    JSGarbageCollect(ctx);
    Require(EvalBool(ctx, "new OtherRegExp('(late)').test('late') && OtherRegExp.$1 === 'late' && OtherRegExp.leftContext === '' && RegExp.$1 === 'mine'"));
}

}  // namespace

int main() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    Eval(ctx, kSnapshot);
    TestBeforeAnyMatch(ctx);
    TestWhatAMatchRecords(ctx);
    TestEveryWayToMatch(ctx);
    TestInputAndMultiline(ctx);
    TestThePropertiesThemselves(ctx);
    TestEachContextHasItsOwn(ctx);
    JSGlobalContextRelease(ctx);
    std::puts("libSceNKWebKit JS RegExp statics tests passed");
    return 0;
}
