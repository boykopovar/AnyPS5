#include "../src/Jsc.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceJsc/include/Api.hpp"
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>

using namespace NKWebKit;

namespace {

using Clock = std::chrono::steady_clock;

constexpr double LimitSeconds = 0.0625;
constexpr auto Limit = std::chrono::microseconds(62500);

constexpr const char* Setup =
    "globalThis.finished = 0; globalThis.spinMs = 3000;"
    "function spin() { var end = Date.now() + spinMs; while (Date.now() < end) {} finished++; }"
    "globalThis.slow = {"
    "  valueOf() { spin(); return 1; }, toString() { spin(); return 'x'; }, toJSON() { spin(); return 1; },"
    "  get prop() { spin(); return 1; }, set prop(v) { spin(); },"
    "  get 0() { spin(); return 1; }, set 0(v) { spin(); }"
    "};"
    "globalThis.trap = new Proxy({}, {"
    "  get() { spin(); return 1; }, set() { spin(); return true; }, has() { spin(); return true; },"
    "  deleteProperty() { spin(); return true; }, getPrototypeOf() { spin(); return null; },"
    "  setPrototypeOf() { spin(); return true; }, ownKeys() { spin(); return []; }"
    "});"
    "globalThis.SlowKind = function () {};"
    "Object.defineProperty(SlowKind, Symbol.hasInstance, { value() { spin(); return true; } }); 0";

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JscTimeLimitEntries check failed at line %u\n", location.line());
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

JSObjectRef EvalObject(JSContextRef ctx, const char* code, std::source_location location = std::source_location::current()) {
    JSObjectRef object = JSValueToObject(ctx, Eval(ctx, code, location), nullptr);
    Require(object != nullptr, location);
    return object;
}

struct Fixture {
    JSContextGroupRef group;
    JSGlobalContextRef ctx;
    JSObjectRef slow = nullptr;
    JSObjectRef trap = nullptr;
    JSObjectRef kind = nullptr;
    JSValueRef one = nullptr;

    Fixture() : group(JSContextGroupCreate()), ctx(JSGlobalContextCreateInGroup(group, nullptr)) {
        Require(group != nullptr && ctx != nullptr);
        Eval(ctx, Setup);
        slow = EvalObject(ctx, "slow");
        trap = EvalObject(ctx, "trap");
        kind = EvalObject(ctx, "SlowKind");
        one = JSValueMakeNumber(ctx, 1.0);
        JSValueProtect(ctx, slow);
        JSValueProtect(ctx, trap);
        JSValueProtect(ctx, kind);
    }
    Fixture(const Fixture&) = delete;
    ~Fixture() {
        JSGlobalContextRelease(ctx);
        JSContextGroupRelease(group);
    }
    OpaqueJSContextGroup* State() const { return const_cast<OpaqueJSContextGroup*>(group); }
    double Finished() const { return JSValueToNumber(ctx, Eval(ctx, "finished"), nullptr); }
};

template <typename Call>
void RequireLimited(const Fixture& f, Call call, std::source_location location = std::source_location::current()) {
    const double before = f.Finished();
    const auto start = Clock::now();
    call();
    Require(Clock::now() - start >= Limit, location);
    Require(f.State()->hasTimeLimit && f.State()->vmEntryDepth == 0 && !f.State()->deadlineArmed, location);
    Require(f.Finished() == before, location);
}

template <typename Call>
void RequireCompleted(const Fixture& f, Call call, std::source_location location = std::source_location::current()) {
    const double before = f.Finished();
    call();
    Require(f.State()->vmEntryDepth == 0 && !f.State()->deadlineArmed, location);
    Require(f.Finished() == before + 1.0, location);
}

void TestValueExportsAreLimited() {
    Fixture f;
    JSContextGroupSetExecutionTimeLimit(f.group, LimitSeconds, nullptr, nullptr);
    JSValueRef exception = nullptr;

    RequireLimited(f, [&] { Require(std::isnan(JSValueToNumber(f.ctx, f.slow, &exception))); });
    Require(exception != nullptr);
    exception = nullptr;
    RequireLimited(f, [&] { Require(JSValueToStringCopy(f.ctx, f.slow, &exception) == nullptr); });
    Require(exception != nullptr);
    exception = nullptr;
    RequireLimited(f, [&] { Require(JSValueCreateJSONString(f.ctx, f.slow, 0, &exception) == nullptr); });
    Require(exception != nullptr);
    exception = nullptr;
    RequireLimited(f, [&] { Require(!JSValueIsEqual(f.ctx, f.slow, f.one, &exception)); });
    Require(exception != nullptr);
    exception = nullptr;
    RequireLimited(f, [&] { Require(!JSValueIsInstanceOfConstructor(f.ctx, f.one, f.kind, &exception)); });
    Require(exception != nullptr);

    JSValueRef argument = f.slow;
    exception = nullptr;
    RequireLimited(f, [&] { Require(JSObjectMakeDate(f.ctx, 1, &argument, &exception) == nullptr); });
    Require(exception != nullptr);
    exception = nullptr;
    RequireLimited(f, [&] { Require(JSObjectMakeError(f.ctx, 1, &argument, &exception) == nullptr); });
    Require(exception != nullptr);
    exception = nullptr;
    RequireLimited(f, [&] { Require(JSObjectMakeRegExp(f.ctx, 1, &argument, &exception) == nullptr); });
    Require(exception != nullptr);
}

void TestPropertyExportsAreLimited() {
    Fixture f;
    JSContextGroupSetExecutionTimeLimit(f.group, LimitSeconds, nullptr, nullptr);
    ScopedJsString name("prop");
    JSValueRef key = Eval(f.ctx, "'prop'");
    JSValueRef exception = nullptr;

    RequireLimited(f, [&] { Require(JSObjectGetProperty(f.ctx, f.slow, name, &exception) == nullptr); });
    Require(exception != nullptr);
    exception = nullptr;
    RequireLimited(f, [&] { JSObjectSetProperty(f.ctx, f.slow, name, f.one, kJSPropertyAttributeNone, &exception); });
    Require(exception != nullptr);
    RequireLimited(f, [&] { Require(!JSObjectHasProperty(f.ctx, f.trap, name)); });
    exception = nullptr;
    RequireLimited(f, [&] { Require(!JSObjectDeleteProperty(f.ctx, f.trap, name, &exception)); });
    Require(exception != nullptr);

    exception = nullptr;
    RequireLimited(f, [&] { Require(JSObjectGetPropertyAtIndex(f.ctx, f.slow, 0, &exception) == nullptr); });
    Require(exception != nullptr);
    exception = nullptr;
    RequireLimited(f, [&] { JSObjectSetPropertyAtIndex(f.ctx, f.slow, 0, f.one, &exception); });
    Require(exception != nullptr);

    exception = nullptr;
    RequireLimited(f, [&] { Require(JSObjectGetPropertyForKey(f.ctx, f.slow, key, &exception) == nullptr); });
    Require(exception != nullptr);
    exception = nullptr;
    RequireLimited(f, [&] { JSObjectSetPropertyForKey(f.ctx, f.slow, key, f.one, kJSPropertyAttributeNone, &exception); });
    Require(exception != nullptr);
    exception = nullptr;
    RequireLimited(f, [&] { Require(!JSObjectDeletePropertyForKey(f.ctx, f.trap, key, &exception)); });
    Require(exception != nullptr);

    RequireLimited(f, [&] { JSObjectGetPrototype(f.ctx, f.trap); });
    RequireLimited(f, [&] { JSObjectSetPrototype(f.ctx, f.trap, JSValueMakeNull(f.ctx)); });
    RequireLimited(f, [&] {
        JSPropertyNameArrayRef names = JSObjectCopyPropertyNames(f.ctx, f.trap);
        if (names) JSPropertyNameArrayRelease(names);
    });
}

void TestQuickWorkStillCompletesUnderTheLimit() {
    Fixture f;
    Eval(f.ctx, "spinMs = 0; 0");
    JSContextGroupSetExecutionTimeLimit(f.group, LimitSeconds, nullptr, nullptr);
    ScopedJsString name("prop");
    JSValueRef exception = nullptr;

    RequireCompleted(f, [&] { Require(JSValueToNumber(f.ctx, f.slow, &exception) == 1.0); });
    RequireCompleted(f, [&] {
        JSStringRef text = JSValueToStringCopy(f.ctx, f.slow, &exception);
        Require(text != nullptr);
        JSStringRelease_nid_postfix(text);
    });
    RequireCompleted(f, [&] { Require(JSValueIsEqual(f.ctx, f.slow, f.one, &exception)); });
    RequireCompleted(f, [&] { Require(JSValueIsInstanceOfConstructor(f.ctx, f.one, f.kind, &exception)); });
    RequireCompleted(f, [&] { Require(JSObjectGetProperty(f.ctx, f.slow, name, &exception) != nullptr); });
    RequireCompleted(f, [&] { Require(JSObjectHasProperty(f.ctx, f.trap, name)); });
    RequireCompleted(f, [&] { Require(JSObjectDeleteProperty(f.ctx, f.trap, name, &exception)); });
    Require(exception == nullptr);
}

OpaqueJSContextGroup* nestedGroup = nullptr;
bool nestedKeptTheOuterDeadline = false;
double nestedValue = 0.0;

JSValueRef APS5_VABI Nested(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, std::size_t argumentCount, const JSValueRef arguments[],
    JSValueRef* exception) {
    (void)function;
    (void)thisObject;
    (void)exception;
    Require(argumentCount == 1);
    const auto deadline = nestedGroup->deadline;
    const int depth = nestedGroup->vmEntryDepth;
    nestedValue = JSValueToNumber(ctx, arguments[0], nullptr);
    nestedKeptTheOuterDeadline =
        nestedGroup->deadline == deadline && nestedGroup->vmEntryDepth == depth && depth == 1 && nestedGroup->deadlineArmed;
    return JSValueMakeUndefined(ctx);
}

void TestNestedExportKeepsTheOuterDeadline() {
    Fixture f;
    Eval(f.ctx, "spinMs = 0; 0");
    nestedGroup = f.State();
    ScopedJsString name("nested");
    JSObjectRef function = JSObjectMakeFunctionWithCallback(f.ctx, name, &Nested);
    Require(function != nullptr);
    JSObjectSetProperty(f.ctx, JSContextGetGlobalObject(f.ctx), name, function, kJSPropertyAttributeNone, nullptr);
    JSContextGroupSetExecutionTimeLimit(f.group, 60.0, nullptr, nullptr);
    Eval(f.ctx, "nested(slow); 0");
    Require(nestedValue == 1.0);
    Require(nestedKeptTheOuterDeadline);
    Require(f.State()->vmEntryDepth == 0 && !f.State()->deadlineArmed);
    nestedGroup = nullptr;
}

void TestCopyPropertyNamesStopsAtAnException() {
    Fixture f;
    JSObjectRef throwing = EvalObject(f.ctx,
        "globalThis.prototypeAsked = 0;"
        "new Proxy({}, { ownKeys() { throw new Error('no keys'); }, getPrototypeOf() { prototypeAsked++; return null; } })");
    JSPropertyNameArrayRef names = JSObjectCopyPropertyNames(f.ctx, throwing);
    Require(names != nullptr);
    Require(JSPropertyNameArrayGetCount(names) == 0);
    JSPropertyNameArrayRelease(names);
    Require(JSValueToNumber(f.ctx, Eval(f.ctx, "prototypeAsked"), nullptr) == 0.0);

    JSObjectRef plain = EvalObject(f.ctx,
        "new Proxy({ own: 1 }, { getPrototypeOf() { prototypeAsked++; return { inherited: 2 }; } })");
    names = JSObjectCopyPropertyNames(f.ctx, plain);
    Require(names != nullptr);
    Require(JSPropertyNameArrayGetCount(names) == 2);
    JSPropertyNameArrayRelease(names);
    Require(JSValueToNumber(f.ctx, Eval(f.ctx, "prototypeAsked"), nullptr) == 1.0);
}

}  // namespace

int main() {
    TestCopyPropertyNamesStopsAtAnException();
    TestValueExportsAreLimited();
    TestPropertyExportsAreLimited();
    TestQuickWorkStillCompletesUnderTheLimit();
    TestNestedExportKeepsTheOuterDeadline();
    std::puts("libSceNKWebKit JS execution time limit entry tests passed");
    return 0;
}
