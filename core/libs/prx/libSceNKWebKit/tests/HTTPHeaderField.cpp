#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

using NKWebKit::WebCoreAbi::HTTPHeaderField;
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

int main() {
    TestMoveConstruct();
    TestNullStrings();
    TestCopyAndDestroy();
    TestAssign();
    TestNull();
    return 0;
}
