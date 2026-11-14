#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <stdexcept>

using NKWebKit::WebCoreAbi::HTTPHeaderField;
using NKWebKit::WebCoreAbi::OptionalHTTPHeaderField;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "HTTPHeaderField check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static StringImpl Make(const char* text) {
    return StringImpl{4, static_cast<unsigned>(std::strlen(text)), text, 4};
}

template <typename Function>
static bool Throws(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static void TestMoveConstruct() {
    auto name = Make("Content-Type");
    auto value = Make("text/html");
    String nameString(&name);
    String valueString(&value);

    alignas(HTTPHeaderField) unsigned char storage[sizeof(HTTPHeaderField)];
    auto* field = reinterpret_cast<HTTPHeaderField*>(storage);
    _ZN7WebCore15HTTPHeaderFieldC1EON3WTF6StringES3_(field, &nameString, &valueString);
    Require(field->name.impl == &name && field->value.impl == &value);
    Require(nameString.impl == nullptr && valueString.impl == nullptr);
    Require(name.refCount == 4 && value.refCount == 4);
    Require(&_ZNK7WebCore15HTTPHeaderField4nameEv(field) == &field->name);
    Require(&_ZNK7WebCore15HTTPHeaderField5valueEv(field) == &field->value);

    String nameAgain(&name);
    String valueAgain(&value);
    alignas(HTTPHeaderField) unsigned char otherStorage[sizeof(HTTPHeaderField)];
    auto* second = reinterpret_cast<HTTPHeaderField*>(otherStorage);
    _ZN7WebCore15HTTPHeaderFieldC2EON3WTF6StringES3_(second, &nameAgain, &valueAgain);
    Require(second->name.impl == &name && second->value.impl == &value);
    Require(nameAgain.impl == nullptr && valueAgain.impl == nullptr);
    Require(name.refCount == 4 && value.refCount == 4);
    second->name.impl = nullptr;
    second->value.impl = nullptr;
    field->name.impl = nullptr;
    field->value.impl = nullptr;
}

static void TestNullStrings() {
    String nullName;
    auto value = Make("1");
    String valueString(&value);
    alignas(HTTPHeaderField) unsigned char storage[sizeof(HTTPHeaderField)];
    auto* field = reinterpret_cast<HTTPHeaderField*>(storage);
    _ZN7WebCore15HTTPHeaderFieldC1EON3WTF6StringES3_(field, &nullName, &valueString);
    Require(field->name.impl == nullptr && field->value.impl == &value);
    value.refCount = 6;
    _ZN7WebCore15HTTPHeaderFieldD1Ev(field);
    Require(field->name.impl == nullptr && field->value.impl == nullptr);
    Require(value.refCount == 4);
}

static void TestCopyAndDestroy() {
    auto name = Make("Accept");
    auto value = Make("*/*");
    alignas(HTTPHeaderField) unsigned char storage[sizeof(HTTPHeaderField)];
    auto* original = reinterpret_cast<HTTPHeaderField*>(storage);
    original->name.impl = &name;
    original->value.impl = &value;

    alignas(HTTPHeaderField) unsigned char copyStorage[sizeof(HTTPHeaderField)];
    auto* copy = reinterpret_cast<HTTPHeaderField*>(copyStorage);
    _ZN7WebCore15HTTPHeaderFieldC1ERKS0_(copy, original);
    Require(copy->name.impl == &name && copy->value.impl == &value);
    Require(name.refCount == 6 && value.refCount == 6);

    alignas(HTTPHeaderField) unsigned char secondStorage[sizeof(HTTPHeaderField)];
    auto* second = reinterpret_cast<HTTPHeaderField*>(secondStorage);
    _ZN7WebCore15HTTPHeaderFieldC2ERKS0_(second, original);
    Require(name.refCount == 8 && value.refCount == 8);

    _ZN7WebCore15HTTPHeaderFieldD1Ev(second);
    Require(name.refCount == 6 && value.refCount == 6);
    Require(second->name.impl == nullptr && second->value.impl == nullptr);
    _ZN7WebCore15HTTPHeaderFieldD2Ev(copy);
    Require(name.refCount == 4 && value.refCount == 4);
    original->name.impl = nullptr;
    original->value.impl = nullptr;
}

static void TestAssign() {
    auto name = Make("Host");
    auto value = Make("example.com");
    auto otherName = Make("Accept");
    auto otherValue = Make("text/plain");
    name.refCount = value.refCount = 6;
    alignas(HTTPHeaderField) unsigned char storage[sizeof(HTTPHeaderField)];
    auto* left = reinterpret_cast<HTTPHeaderField*>(storage);
    left->name.impl = &name;
    left->value.impl = &value;
    alignas(HTTPHeaderField) unsigned char otherStorage[sizeof(HTTPHeaderField)];
    auto* right = reinterpret_cast<HTTPHeaderField*>(otherStorage);
    right->name.impl = &otherName;
    right->value.impl = &otherValue;

    auto& result = _ZN7WebCore15HTTPHeaderFieldaSERKS0_(left, right);
    Require(&result == left);
    Require(left->name.impl == &otherName && left->value.impl == &otherValue);
    Require(otherName.refCount == 6 && otherValue.refCount == 6);
    Require(name.refCount == 4 && value.refCount == 4);

    _ZN7WebCore15HTTPHeaderFieldaSERKS0_(left, left);
    Require(left->name.impl == &otherName && left->value.impl == &otherValue);
    Require(otherName.refCount == 6 && otherValue.refCount == 6);

    right->name.impl = nullptr;
    right->value.impl = nullptr;
    _ZN7WebCore15HTTPHeaderFieldaSERKS0_(left, right);
    Require(left->name.impl == nullptr && left->value.impl == nullptr);
    Require(otherName.refCount == 4 && otherValue.refCount == 4);
}

static void TestNull() {
    alignas(HTTPHeaderField) unsigned char storage[sizeof(HTTPHeaderField)];
    auto* field = reinterpret_cast<HTTPHeaderField*>(storage);
    String nameString;
    Require(Throws([&] { _ZNK7WebCore15HTTPHeaderField4nameEv(nullptr); }));
    Require(Throws([&] { _ZNK7WebCore15HTTPHeaderField5valueEv(nullptr); }));
    Require(Throws([&] { _ZN7WebCore15HTTPHeaderFieldD1Ev(nullptr); }));
    Require(Throws([&] { _ZN7WebCore15HTTPHeaderFieldD2Ev(nullptr); }));
    Require(Throws([&] { _ZN7WebCore15HTTPHeaderFieldC1ERKS0_(field, nullptr); }));
    Require(Throws([&] { _ZN7WebCore15HTTPHeaderFieldC2ERKS0_(nullptr, field); }));
    Require(Throws([&] { _ZN7WebCore15HTTPHeaderFieldaSERKS0_(field, nullptr); }));
    Require(Throws([&] { _ZN7WebCore15HTTPHeaderFieldaSERKS0_(nullptr, field); }));
    Require(Throws([&] { _ZN7WebCore15HTTPHeaderFieldC1EON3WTF6StringES3_(field, nullptr, &nameString); }));
    Require(Throws([&] { _ZN7WebCore15HTTPHeaderFieldC2EON3WTF6StringES3_(field, &nameString, nullptr); }));
    Require(Throws([&] { _ZN7WebCore15HTTPHeaderFieldC1EON3WTF6StringES3_(nullptr, &nameString, &nameString); }));
}

static bool Equals(const String& string, std::string_view expected) {
    if (!string.impl || string.impl->length != expected.size()) return false;
    if (string.impl->hashAndFlags & 4) return std::memcmp(string.impl->data, expected.data(), expected.size()) == 0;
    auto* characters = static_cast<const char16_t*>(string.impl->data);
    for (std::size_t i = 0; i < expected.size(); ++i) {
        if (characters[i] != static_cast<unsigned char>(expected[i])) return false;
    }
    return true;
}

static OptionalHTTPHeaderField Create(StringImpl& name, StringImpl& value) {
    String nameString(&name);
    String valueString(&value);
    auto result = _ZN7WebCore15HTTPHeaderField6createEON3WTF6StringES3_(&nameString, &valueString);
    nameString.impl = nullptr;
    valueString.impl = nullptr;
    return result;
}

static bool Rejects(const char* name, const char* value) {
    auto nameImpl = Make(name);
    auto valueImpl = Make(value);
    auto result = Create(nameImpl, valueImpl);
    bool rejected = !result.hasValue && nameImpl.refCount == 4 && valueImpl.refCount == 4;
    if (result.hasValue) {
        result.field.name.impl = nullptr;
        result.field.value.impl = nullptr;
    }
    return rejected;
}

static bool Accepts(const char* value) {
    auto nameImpl = Make("X");
    auto valueImpl = Make(value);
    auto result = Create(nameImpl, valueImpl);
    bool accepted = result.hasValue && result.field.name.impl == &nameImpl && result.field.value.impl == &valueImpl;
    if (result.hasValue) {
        result.field.name.impl = nullptr;
        result.field.value.impl = nullptr;
    }
    return accepted;
}

static void TestCreateMovesUntrimmedStrings() {
    auto name = Make("Content-Type");
    auto value = Make("text/html; charset=utf-8");
    String nameString(&name);
    String valueString(&value);
    auto result = _ZN7WebCore15HTTPHeaderField6createEON3WTF6StringES3_(&nameString, &valueString);
    Require(result.hasValue);
    Require(result.field.name.impl == &name && result.field.value.impl == &value);
    Require(nameString.impl == nullptr && valueString.impl == nullptr);
    Require(name.refCount == 4 && value.refCount == 4);
    result.field.name.impl = nullptr;
    result.field.value.impl = nullptr;
}

static void TestCreateTrimsWhitespace() {
    auto name = Make("  X-Foo\t");
    auto value = Make(" bar ");
    String nameString(&name);
    String valueString(&value);
    {
        auto result = _ZN7WebCore15HTTPHeaderField6createEON3WTF6StringES3_(&nameString, &valueString);
        Require(result.hasValue);
        Require(result.field.name.impl != &name && result.field.value.impl != &value);
        Require(Equals(result.field.name, "X-Foo") && Equals(result.field.value, "bar"));
        Require(nameString.impl == &name && valueString.impl == &value);
        Require(name.refCount == 4 && value.refCount == 4);
        _ZN7WebCore15HTTPHeaderFieldD1Ev(&result.field);
    }
    nameString.impl = nullptr;
    valueString.impl = nullptr;

    auto paddedName = Make("\tAccept");
    auto plainValue = Make("*/*");
    String paddedString(&paddedName);
    String plainString(&plainValue);
    auto mixed = _ZN7WebCore15HTTPHeaderField6createEON3WTF6StringES3_(&paddedString, &plainString);
    Require(mixed.hasValue);
    Require(Equals(mixed.field.name, "Accept") && mixed.field.name.impl != &paddedName);
    Require(mixed.field.value.impl == &plainValue && plainString.impl == nullptr);
    Require(paddedString.impl == &paddedName);
    paddedString.impl = nullptr;
    mixed.field.value.impl = nullptr;
    _ZN7WebCore15HTTPHeaderFieldD1Ev(&mixed.field);
}

static void TestCreate16BitStrings() {
    const char16_t nameCharacters[] = u" Accept ";
    const char16_t valueCharacters[] = u"\"caf\u00e9\"";
    StringImpl name{4, 8, nameCharacters, 0};
    StringImpl value{4, 6, valueCharacters, 0};
    auto result = Create(name, value);
    Require(result.hasValue);
    Require(Equals(result.field.name, "Accept"));
    Require(result.field.value.impl == &value);
    result.field.value.impl = nullptr;
    _ZN7WebCore15HTTPHeaderFieldD1Ev(&result.field);

    const char16_t wideValue[] = u"\"\u0100\"";
    StringImpl acceptName{4, 6, u"Accept", 0};
    StringImpl wide{4, 3, wideValue, 0};
    auto rejected = Create(acceptName, wide);
    Require(!rejected.hasValue);
}

static void TestCreateRejectsInvalidFields() {
    Require(Rejects("", "v"));
    Require(Rejects(" \t ", "v"));
    Require(Rejects("a b", "v"));
    Require(Rejects("na:me", "v"));
    Require(Rejects("name", ""));
    Require(Rejects("name", " \t "));
    Require(Rejects("name", "\"unterminated"));
    Require(Rejects("name", "(open"));
    Require(Rejects("name", "\"a\\"));
    Require(Rejects("name", "\x7f"));
    Require(Rejects("name", "@value"));
    Require(Rejects("name", "\"bad\x7f\""));
    Require(Rejects("name", "(bad\x7f)"));
    Require(Rejects("name", "(a\\"));

    String nullName;
    String nullValue;
    auto value = Make("v");
    String valueString(&value);
    auto nullNameResult = _ZN7WebCore15HTTPHeaderField6createEON3WTF6StringES3_(&nullName, &valueString);
    Require(!nullNameResult.hasValue && valueString.impl == &value);
    valueString.impl = nullptr;
    auto name = Make("name");
    String nameString(&name);
    auto nullValueResult = _ZN7WebCore15HTTPHeaderField6createEON3WTF6StringES3_(&nameString, &nullValue);
    Require(!nullValueResult.hasValue && nameString.impl == &name);
    nameString.impl = nullptr;
}

static void TestCreateValueGrammar() {
    Require(Accepts("token"));
    Require(Accepts("a, b; q=0.5"));
    Require(Accepts("(c (nested))"));
    Require(Accepts("\"quoted \\\" string\""));
    Require(Accepts("\"obs \x80\xff\""));
    Require(Accepts("(obs \x80\xff)"));
    Require(Accepts("a\x7f"));
    Require(Accepts("a\"b\" c"));
    Require(Accepts("\"x\"(y)"));
}

int main() {
    TestMoveConstruct();
    TestNullStrings();
    TestCopyAndDestroy();
    TestAssign();
    TestNull();
    TestCreateMovesUntrimmedStrings();
    TestCreateTrimsWhitespace();
    TestCreate16BitStrings();
    TestCreateRejectsInvalidFields();
    TestCreateValueGrammar();
    return 0;
}
