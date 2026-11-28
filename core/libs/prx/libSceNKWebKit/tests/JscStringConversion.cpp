#include "../src/Jsc.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceJsc/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <vector>

using namespace NKWebKit;

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "JSC string conversion check failed at line %u\n", location.line());
    std::abort();
}

struct ScopedJsString {
    JSStringRef ref = nullptr;
    explicit ScopedJsString(const char* utf8) : ref(JSStringCreateWithUTF8CString_nid_postfix(utf8)) {}
    explicit ScopedJsString(const std::vector<std::uint16_t>& units)
        : ref(JSStringCreateWithCharacters_nid_postfix(units.data(), units.size())) {}
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

void TestPropertyNames() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    JSObjectRef object = JSObjectMake(ctx, nullptr, nullptr);
    Require(object != nullptr);
    SetGlobal(ctx, "o", object);

    ScopedJsString pair(std::vector<std::uint16_t>{'p', 0xD83D, 0xDE00});
    JSObjectSetProperty(ctx, object, pair, JSValueMakeNumber(ctx, 1.0), kJSPropertyAttributeNone, nullptr);
    Require(EvalBool(ctx, "Object.keys(o).length === 1 && o['p\\u{1F600}'] === 1"));
    Require(JSObjectHasProperty(ctx, object, pair));

    ScopedJsString unpaired(std::vector<std::uint16_t>{'k', 0xD800, 'z'});
    JSValueRef exception = nullptr;
    JSObjectSetProperty(ctx, object, unpaired, JSValueMakeNumber(ctx, 7.0), kJSPropertyAttributeNone, &exception);
    Require(exception == nullptr);
    Require(JSObjectHasProperty(ctx, object, unpaired));
    Require(JSValueToNumber(ctx, JSObjectGetProperty(ctx, object, unpaired, nullptr), nullptr) == 7.0);
    Require(EvalBool(ctx, "Object.keys(o).length === 2 && Object.keys(o)[1].length === 3 && Object.keys(o)[1].charCodeAt(1) === 0xD800"));
    ScopedJsString cut("k");
    Require(!JSObjectHasProperty(ctx, object, cut));

    JSGlobalContextRelease(ctx);
}

void Append(std::vector<std::uint16_t>& units, const char* text) {
    for (; *text != '\0'; ++text) units.push_back(static_cast<std::uint16_t>(static_cast<unsigned char>(*text)));
}

void TestEveryEncodedLength() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    std::vector<std::uint16_t> units;
    Append(units, "globalThis.text = '");
    for (std::uint16_t unit : {0x7F, 0x80, 0xE9, 0x7FF, 0x800, 0x20AC, 0xFFFD, 0xD800, 0xDC00, 0xD83D, 0xDE00, 0xDBFF, 0xDFFF})
        units.push_back(unit);
    Append(units, "'; 0");
    ScopedJsString script(units);
    JSValueRef exception = nullptr;
    Require(JSEvaluateScript(ctx, script, nullptr, nullptr, 1, &exception) != nullptr && exception == nullptr);
    Require(EvalBool(ctx, "text.length === 13"));
    Require(EvalBool(ctx,
        "[0x7F, 0x80, 0xE9, 0x7FF, 0x800, 0x20AC, 0xFFFD].every(function (code, index) { return text.charCodeAt(index) === code; })"));
    Require(EvalBool(ctx, "text.codePointAt(7) === 0x10000 && text.codePointAt(9) === 0x1F600 && text.codePointAt(11) === 0x10FFFF"));
    JSGlobalContextRelease(ctx);
}

void TestScriptSource() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    std::vector<std::uint16_t> units;
    for (const char* text = "globalThis.ran = 5;"; *text != '\0'; ++text) units.push_back(static_cast<std::uint16_t>(*text));
    units.push_back(0xD800);
    for (const char* text = "globalThis.ran = 6;"; *text != '\0'; ++text) units.push_back(static_cast<std::uint16_t>(*text));
    ScopedJsString script(units);
    JSValueRef exception = nullptr;
    JSValueRef value = JSEvaluateScript(ctx, script, nullptr, nullptr, 1, &exception);
    Require(value != nullptr && exception == nullptr);
    Require(EvalBool(ctx, "ran === 5"));
    JSGlobalContextRelease(ctx);
}

void TestSymbolDescription() {
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    ScopedJsString whole(std::vector<std::uint16_t>{'d', 0xD83D, 0xDE00});
    SetGlobal(ctx, "s", JSValueMakeSymbol(ctx, whole));
    Require(EvalBool(ctx, "s.description === 'd\\u{1F600}'"));
    for (const std::vector<std::uint16_t>& units : {std::vector<std::uint16_t>{'d', 0xD800, 'z'},
             std::vector<std::uint16_t>{'d', 0xDC00, 'z'}, std::vector<std::uint16_t>{'d', 0xD800}}) {
        ScopedJsString broken(units);
        SetGlobal(ctx, "s", JSValueMakeSymbol(ctx, broken));
        Require(EvalBool(ctx, "s.description === 'd'"));
    }
    JSGlobalContextRelease(ctx);
}

}  // namespace

int main() {
    TestPropertyNames();
    TestEveryEncodedLength();
    TestScriptSource();
    TestSymbolDescription();
    std::puts("libSceNKWebKit JS string conversion tests passed");
    return 0;
}
