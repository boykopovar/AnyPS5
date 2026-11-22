#include "../src/Jsc.hpp"
#include "prx/libSceNKWebKit/include/InventoryExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceJsc/include/Api.hpp"
#include "prx/libSceJsc/include/ApiBridgeExports.hpp"
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <string>

using namespace NKWebKit;

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JSC date parsing check failed at line %u\n", location.line());
    std::abort();
}

void RequireFor(bool condition, const char* text, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JSC date parsing check failed at line %u for \"%s\"\n", location.line(), text);
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

double CallWithText(JSContextRef ctx, const char* functionSource, const char* text) {
    JSObjectRef function = JSValueToObject(ctx, Eval(ctx, functionSource), nullptr);
    ScopedJsString string(text);
    JSValueRef argument = static_cast<JSValueRef>(JSValueMakeString(ctx, string));
    JSValueRef exception = nullptr;
    JSValueRef result = JSObjectCallAsFunction(ctx, function, nullptr, 1, &argument, &exception);
    Require(exception == nullptr && result != nullptr);
    return JSValueToNumber(ctx, result, nullptr);
}

bool Same(double a, double b) {
    return (std::isnan(a) && std::isnan(b)) || a == b;
}

bool ParsesTo(JSContextRef ctx, const char* text, double expected) {
    return Same(CallWithText(ctx, "(function (s) { return Date.parse(s); })", text), expected) && Same(CallWithText(ctx, "(function (s) { return new Date(s).getTime(); })", text), expected);
}

struct Absolute {
    const char* text;
    double time;
};

const Absolute kAbsolute[] = {
    {"2021-01-02", 1609545600000.0},
    {"2021-01-02T10:00:00Z", 1609581600000.0},
    {"2021-01-02T10:00:00+01:00", 1609578000000.0},
    {"2021-01-02T10:00:00+0100", 1609578000000.0},
    {"2021-01", 1609459200000.0},
    {"2021", 1609459200000.0},
    {"+002021-01-02T00:00:00Z", 1609545600000.0},
    {"2021-01-02T24:00:00Z", 1609632000000.0},
    {"2021-01-02T10:00:00.123456Z", 1609581600123.0},
    {"2021-01-02T10:00:00.1Z", 1609581600100.0},
    {"2021-01-02t10:00:00Z", 1609581600000.0},
    {"-000001-01-01T00:00:00Z", -62198755200000.0},
    {"275760-09-13", 8640000000000000.0},
    {"+275760-09-13T00:00:00.000Z", 8640000000000000.0},
    {"+275760-09-13T00:00:00.001Z", kNaN},
    {"0", -62167219200000.0},
    {"12", -61788528000000.0},
    {"1994", 757382400000.0},
    {"Sat, 02 Jan 2021 10:00:00 GMT", 1609581600000.0},
    {"Sat Jan 02 2021 10:00:00 GMT+0100 (CET)", 1609578000000.0},
    {"2 Jan 2021 10:00:00 UTC", 1609581600000.0},
    {"2 Jan 2021 10:00:00 UT", 1609581600000.0},
    {"2 Jan 2021 10:00:00 +0530", 1609561800000.0},
    {"2 Jan 2021 10:00:00 -08:00", 1609610400000.0},
    {"2 Jan 2021 10:00:00 EST", 1609599600000.0},
    {"2 Jan 2021 10:00:00 PDT", 1609606800000.0},
    {"2 Jan 2021 10:00:00 GMT+1", 1609578000000.0},
    {"2 Jan 2021 10:00:00 GMT+0130", 1609576200000.0},
    {"Tue, 15 Nov 1994 08:12:31 GMT", 784887151000.0},
    {"Tuesday, 15-Nov-94 08:12:31 GMT", 784887151000.0},
    {"Mon, 25 Dec 1995 13:30:00 +0430", 819882000000.0},
    {"2021-01-02 10:00:00Z", 1609581600000.0},
    {"Sep 13 275760 00:00:00 GMT", 8640000000000000.0},
    {"Sep 13 275760 00:00:01 GMT", kNaN},
    {"Jan 1 1970 00:00:00 GMT", 0.0},
    {"Jan 1 1969 GMT", -31536000000.0},
    {"Jan 1 0099 GMT", 915148800000.0},
    {"Jan 1 99 GMT", 915148800000.0},
    {"Jan 1 49 GMT", 2493072000000.0},
    {"Jan 1 50 GMT", -631152000000.0},
    {"Wed Dec 31 1969 16:00:00 GMT-0800 (PST)", 0.0},
    {"2021-01-02T10:00:00z", kNaN},
    {"2021-02-30", kNaN},
    {"2021-13-01", kNaN},
    {"2021-1-2", kNaN},
    {"13/01/2021", kNaN},
    {"2021.01.02", kNaN},
    {"20210102", kNaN},
    {"Jan 2 2021 13:00 PM", kNaN},
    {"Jan 2 2021 25:00", kNaN},
    {"Jan 32 2021", kNaN},
    {"2 Jan 2021 10:00:00 Z", kNaN},
    {"2 Jan 2021 10:00:00 CET", kNaN},
    {"2021-01-02 10:00:00 GMT", kNaN},
    {"2021-01-02 10:00:00 +0100", kNaN},
    {"T10:00", kNaN},
    {"", kNaN},
    {" ", kNaN},
    {"garbage", kNaN},
    {"undefined", kNaN},
    {"1e3", kNaN},
    {"2 2021 Jan", kNaN},
    {"10:00 Jan 2 2021", kNaN},
    {"Jan 2 2021 10:00 Z", kNaN},
    {"Jan 2 2021 10:00:00.500", kNaN},
    {"Jan 2 2021 10:00:00.500 GMT", kNaN},
    {"2021-W01-1", kNaN},
    {"2021-002", kNaN},
    {"Jan 2 2021 at 10:00", kNaN},
};

struct Local {
    const char* text;
    const char* fields;
};

const Local kLocal[] = {
    {"2021-01-02T10:00:00", "2021, 0, 2, 10"},
    {"2021-01-02T10:00", "2021, 0, 2, 10"},
    {"2021/01/02", "2021, 0, 2"},
    {"2021/01/02 10:00:00", "2021, 0, 2, 10"},
    {"01/02/2021", "2021, 0, 2"},
    {"1/2/2021 10:00", "2021, 0, 2, 10"},
    {"1/2/21", "2021, 0, 2"},
    {"12/31/1999 23:59:59", "1999, 11, 31, 23, 59, 59"},
    {"Jan 2 2021", "2021, 0, 2"},
    {"Jan 2, 2021", "2021, 0, 2"},
    {"January 2, 2021 10:00:00", "2021, 0, 2, 10"},
    {"2 January 2021 10:00", "2021, 0, 2, 10"},
    {"Jan 2 21", "2021, 0, 2"},
    {"Jan 2 2021 10:00 PM", "2021, 0, 2, 22"},
    {"Jan 2 2021 12:00 AM", "2021, 0, 2, 0"},
    {"Jan 2 2021 12:00 PM", "2021, 0, 2, 12"},
    {"Feb 30 2021", "2021, 2, 2"},
    {"Sat Jan 02 2021", "2021, 0, 2"},
    {"Saturday, January 2, 2021", "2021, 0, 2"},
    {"Tue Nov 15 08:12:31 1994", "1994, 10, 15, 8, 12, 31"},
    {"Dec 25, 1995 13:30", "1995, 11, 25, 13, 30"},
    {"2021-01-02 10:00:00", "2021, 0, 2, 10"},
    {"(comment) Jan 2 2021", "2021, 0, 2"},
    {"  Jan 2 2021  ", "2021, 0, 2"},
    {"JAN 2 2021", "2021, 0, 2"},
    {"Sept 2 2021", "2021, 8, 2"},
    {"2 janvier 2021", "2021, 0, 2"},
    {"1/1/0001", "2001, 0, 1"},
    {"Thu, 01 Jan 1970 00:00:00", "1970, 0, 1"},
    {"Mar 26 2021 02:30", "2021, 2, 26, 2, 30"},
    {"Mar 28 2021 02:30", "2021, 2, 28, 2, 30"},
    {"Mar 14 2021 02:30", "2021, 2, 14, 2, 30"},
    {"Jul 4 2021 02:30:15", "2021, 6, 4, 2, 30, 15"},
    {"Oct 31 2021 02:30", "2021, 9, 31, 2, 30"},
    {"2021-07-04T23:59:59.999", "2021, 6, 4, 23, 59, 59, 999"},
    {"2021-07-04T23:59:59.9995", "2021, 6, 4, 23, 59, 59, 999"},
    {"1600-02-29T12:00:00", "1600, 1, 29, 12"},
    {"-000100-03-01T00:00:00", "-100, 2, 1"},
    {"2024-02-29T00:00:00", "2024, 1, 29"},
    {"2023-12-31T23:59:59", "2023, 11, 31, 23, 59, 59"},
    {"2021-01-01T00:00:00", "2021, 0, 1"},
};

void TestStringsThatDoNotDependOnTheTimeZone(JSContextRef ctx) {
    for (const Absolute& entry : kAbsolute) {
        RequireFor(ParsesTo(ctx, entry.text, entry.time), entry.text);
        bool isLocalTime = false;
        double fromWtf = _ZN3WTF40parseES5DateFromNullTerminatedCharactersEPKcRb(entry.text, isLocalTime);
        if (std::isnan(fromWtf)) fromWtf = _ZN3WTF37parseDateFromNullTerminatedCharactersEPKcRb(entry.text, &isLocalTime);
        if (!std::isnan(entry.time)) RequireFor(!isLocalTime && std::trunc(fromWtf) == entry.time, entry.text);
    }
}

void TestStringsInLocalTime(JSContextRef ctx) {
    for (const Local& entry : kLocal) {
        const std::string expected = std::string("new Date(") + entry.fields + ").getTime()";
        const double time = JSValueToNumber(ctx, Eval(ctx, expected.c_str()), nullptr);
        RequireFor(!std::isnan(time) && ParsesTo(ctx, entry.text, time), entry.text);
    }
    Require(EvalBool(ctx, "var early = new Date(2000, 5, 15, 12); early.setFullYear(50); Date.parse('0050-06-15T12:00:00') === early.getTime() && new Date('0050-06-15T12:00:00').getFullYear() === 50"));
    Require(EvalBool(ctx, "var first = new Date(2000, 0, 1); first.setFullYear(0); Date.parse('0000-01-01T00:00:00') === first.getTime()"));
    Require(EvalBool(ctx, "var last = new Date(2000, 11, 31, 23, 59, 59); last.setFullYear(99); Date.parse('0099-12-31T23:59:59') === last.getTime()"));
    Require(EvalBool(ctx, "Date.parse('0100-01-01T00:00:00') === new Date(100, 0, 1).getTime()"));
    Require(EvalBool(ctx, "Date.parse('1969-12-31T23:59:59.9995') === Math.trunc(new Date(1970, 0, 1).getTime() - 0.5)"));
    Require(EvalBool(ctx, "Date.parse('1970-01-01T00:00:05.0005') === Math.trunc(new Date(1970, 0, 1, 0, 0, 5).getTime() + 0.5)"));
    Require(EvalBool(ctx, "var d = new Date('Jan 2 2021 10:07:09 PM'); d.getFullYear() === 2021 && d.getMonth() === 0 && d.getDate() === 2 && d.getHours() === 22 && d.getMinutes() === 7 && d.getSeconds() === 9"));
}

void TestWhatTheConstructorDoesWithItsArgument(JSContextRef ctx) {
    Require(EvalBool(ctx, "new Date(new String('Jan 2 2021 GMT')).getTime() === 1609545600000"));
    Require(EvalBool(ctx, "new Date({ valueOf: function () { return 'Jan 2 2021 GMT'; } }).getTime() === 1609545600000"));
    Require(EvalBool(ctx, "new Date({ valueOf: function () { return 5; }, toString: function () { return 'Jan 2 2021 GMT'; } }).getTime() === 5"));
    Require(EvalBool(ctx, "var order = []; new Date({ valueOf: function () { order.push('valueOf'); return {}; }, toString: function () { order.push('toString'); return '1/1/0001'; } }).getFullYear() === 2001 && order.join() === 'valueOf,toString'"));
    Require(EvalBool(ctx, "var hints = []; var hinted = {}; hinted[Symbol.toPrimitive] = function (hint) { hints.push(hint); return 'Jan 1 0099 GMT'; }; new Date(hinted).getTime() === 915148800000 && hints.join() === 'default'"));
    Require(EvalBool(ctx, "var asked = 0; var bad = {}; bad[Symbol.toPrimitive] = function () { ++asked; return {}; }; var thrown = null; try { new Date(bad); } catch (e) { thrown = e; } thrown instanceof TypeError && asked === 1"));
    Require(EvalBool(ctx, "var notCallable = {}; notCallable[Symbol.toPrimitive] = 5; thrown = null; try { new Date(notCallable); } catch (e) { thrown = e; } thrown instanceof TypeError"));
    Require(EvalBool(ctx, "var none = Object.create(null); thrown = null; try { new Date(none); } catch (e) { thrown = e; } thrown instanceof TypeError"));
    Require(EvalBool(ctx, "var raising = { valueOf: function () { throw new RangeError('mine'); } }; thrown = null; try { new Date(raising); } catch (e) { thrown = e; } thrown instanceof RangeError"));
    Require(EvalBool(ctx, "thrown = null; try { new Date(Symbol()); } catch (e) { thrown = e; } thrown instanceof TypeError"));

    Require(EvalBool(ctx, "var source = new Date(1234); source[Symbol.toPrimitive] = function () { throw new Error('asked'); }; source.valueOf = function () { throw new Error('asked'); }; new Date(source).getTime() === 1234"));
    Require(EvalBool(ctx, "isNaN(new Date(undefined).getTime()) && new Date(null).getTime() === 0 && new Date(true).getTime() === 1 && new Date(7.9).getTime() === 7"));
    Require(EvalBool(ctx, "new Date(2021, 0, 2, 10).getHours() === 10 && new Date(2021, 0).getDate() === 1 && typeof new Date().getTime() === 'number'"));
    Require(EvalBool(ctx, "typeof Date() === 'string' && typeof Date('garbage') === 'string' && typeof Date(0) === 'string' && Date(0) !== new Date(0).toString()"));
    Require(EvalBool(ctx, "typeof Date({ valueOf: function () { throw new Error('asked'); } }) === 'string'"));
}

void TestTheConstructorObject(JSContextRef ctx) {
    Require(EvalBool(ctx, "typeof Date === 'function' && Date.length === 7 && Date.name === 'Date' && Object.getPrototypeOf(Date) === Function.prototype && Date instanceof Function"));
    Require(EvalBool(ctx, "Date.prototype.constructor === Date && new Date(0).constructor === Date && new Date(0) instanceof Date && Object.getPrototypeOf(new Date('2021')) === Date.prototype"));
    Require(EvalBool(ctx, "Object.getOwnPropertyNames(Date).sort().join() === 'UTC,length,name,now,parse,prototype'"));
    Require(EvalBool(ctx, "var p = Object.getOwnPropertyDescriptor(Date, 'prototype'); p.writable === false && p.enumerable === false && p.configurable === false"));
    Require(EvalBool(ctx, "var f = Object.getOwnPropertyDescriptor(Date, 'parse'); f.writable === true && f.enumerable === false && f.configurable === true && Date.parse.length === 1 && Date.parse.name === 'parse'"));
    Require(EvalBool(ctx, "var g = Object.getOwnPropertyDescriptor(globalThis, 'Date'); g.writable === true && g.enumerable === false && g.configurable === true"));
    Require(EvalBool(ctx, "var c = Object.getOwnPropertyDescriptor(Date.prototype, 'constructor'); c.writable === true && c.enumerable === false && c.configurable === true"));
    Require(EvalBool(ctx, "Function.prototype.toString.call(Date) === Function.prototype.toString.call(Object).replace('Object', 'Date')"));
    Require(EvalBool(ctx, "Object.prototype.toString.call(Date) === '[object Function]' && Date.UTC(2021, 0, 2) === 1609545600000 && typeof Date.now() === 'number'"));

    Require(EvalBool(ctx, "class Stamp extends Date { tag() { return 't'; } } var stamp = new Stamp('Jan 1 0099 GMT');"
                          "stamp instanceof Stamp && stamp instanceof Date && stamp.tag() === 't' && stamp.getTime() === 915148800000 && new Stamp(2021, 0, 2).getDate() === 2"));
    Require(EvalBool(ctx, "function Other() {} var reflected = Reflect.construct(Date, ['Jan 1 0099 GMT'], Other);"
                          "Object.getPrototypeOf(reflected) === Other.prototype && Date.prototype.getTime.call(reflected) === 915148800000"));

    Require(EvalBool(ctx, "isNaN(Date.parse()) && Date.parse({ toString: function () { return 'Jan 1 0099 GMT'; } }) === 915148800000 && Date.parse(915148800000) !== 915148800000"));
    Require(EvalBool(ctx, "thrown = null; try { Date.parse(Symbol()); } catch (e) { thrown = e; } thrown instanceof TypeError"));
    Require(EvalBool(ctx, "var parse = Date.parse; parse('Jan 1 0099 GMT') === 915148800000 && Date.parse.call(null, '1/1/0001') === new Date(2001, 0, 1).getTime()"));
    Require(EvalBool(ctx, "Date.parse('1/2/2021, 10:00:00\\u202FPM') === new Date(2021, 0, 2, 22).getTime() && Date.parse('2021-01-02\\u0000garbage') === 1609545600000"));

    Require(EvalBool(ctx, "var kept = Date.parse('1/1/0001'); var realSetFullYear = Date.prototype.setFullYear; Date.prototype.setFullYear = function () { throw new Error('replaced'); };"
                          "var same = Date.parse('1/1/0001') === kept && new Date('0050-06-15T12:00:00').getFullYear() === 50; Date.prototype.setFullYear = realSetFullYear; same"));
    Require(EvalBool(ctx, "var RealDate = Date; Date = function () { throw new Error('replaced'); }; var still = RealDate.parse('1/1/0001') === kept; Date = RealDate; still && new Date('1/1/0001').getTime() === kept"));
}

void TestTheApi(JSContextRef ctx) {
    ScopedJsString legacy("1/1/0001");
    JSValueRef argument = static_cast<JSValueRef>(JSValueMakeString(ctx, legacy));
    JSValueRef exception = nullptr;
    JSObjectRef date = JSObjectMakeDate(ctx, 1, &argument, &exception);
    Require(date != nullptr && exception == nullptr && JSValueIsDate(ctx, date));
    Require(JSValueToNumber(ctx, date, nullptr) == JSValueToNumber(ctx, Eval(ctx, "new Date(2001, 0, 1).getTime()"), nullptr));

    ScopedJsString refused("Jan 32 2021");
    JSValueRef refusedArgument = static_cast<JSValueRef>(JSValueMakeString(ctx, refused));
    JSObjectRef invalid = JSObjectMakeDate(ctx, 1, &refusedArgument, &exception);
    Require(invalid != nullptr && exception == nullptr && JSValueIsDate(ctx, invalid) && std::isnan(JSValueToNumber(ctx, invalid, nullptr)));

    JSValueRef fields[3] = {JSValueMakeNumber(ctx, 2021.0), JSValueMakeNumber(ctx, 0.0), JSValueMakeNumber(ctx, 2.0)};
    JSObjectRef fromFields = JSObjectMakeDate(ctx, 3, fields, &exception);
    Require(fromFields != nullptr && JSValueToNumber(ctx, fromFields, nullptr) == JSValueToNumber(ctx, Eval(ctx, "new Date(2021, 0, 2).getTime()"), nullptr));
    Require(JSObjectMakeDate(ctx, 0, nullptr, &exception) != nullptr && exception == nullptr);

    JSContextGroupRef group = JSContextGetGroup(ctx);
    JSGlobalContextRef other = JSGlobalContextCreateInGroup(group, nullptr);
    Require(ParsesTo(other, "Jan 1 0099 GMT", 915148800000.0));
    ScopedJsString name("OtherDate");
    JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), name, JSValueToObject(other, Eval(other, "Date"), nullptr), kJSPropertyAttributeNone, nullptr);
    Require(EvalBool(ctx, "OtherDate !== Date && OtherDate.prototype !== Date.prototype && !(new OtherDate(0) instanceof Date)"));
    JSGlobalContextRelease(other);
    JSGarbageCollect(ctx);
    Require(EvalBool(ctx, "OtherDate.parse('Jan 1 0099 GMT') === 915148800000 && new OtherDate('1/1/0001').getFullYear() === 2001 && typeof OtherDate() === 'string'"));
}

}  // namespace

int main() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    Eval(ctx, "var thrown = null; 0");
    TestStringsThatDoNotDependOnTheTimeZone(ctx);
    TestStringsInLocalTime(ctx);
    TestWhatTheConstructorDoesWithItsArgument(ctx);
    TestTheConstructorObject(ctx);
    TestTheApi(ctx);
    JSGlobalContextRelease(ctx);

    JSClassDefinition definition = kJSClassDefinitionEmpty;
    JSClassRef globalClass = JSClassCreate(&definition);
    JSGlobalContextRef withClass = JSGlobalContextCreate(globalClass);
    JSClassRelease(globalClass);
    Require(ParsesTo(withClass, "Jan 1 0099 GMT", 915148800000.0));
    Require(EvalBool(withClass, "new Date('1/1/0001').getFullYear() === 2001 && Date.prototype.constructor === Date"));
    JSGlobalContextRelease(withClass);
    std::puts("libSceNKWebKit JS date parsing tests passed");
    return 0;
}
