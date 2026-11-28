#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "../src/WtfTextAbi.hpp"
#include <cfloat>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>

static void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "Check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(cond) Check((cond), __LINE__)

using NKWebKit::JsonAbi::Array;
using NKWebKit::JsonAbi::Object;
using NKWebKit::JsonAbi::Ref;
using NKWebKit::JsonAbi::Value;
using NKWebKit::JsonAbi::ValueType;
using NKWebKit::WtfAbi::String;

static std::string Serialize(const Value* value) {
    NKWebKit::JsonAbi::StringBuilder output{String(), nullptr, nullptr, false, true, 0, true};
    if (!value || !value->vptr || !value->vptr->writeJSON) throw std::invalid_argument("invalid JSON value");
    value->vptr->writeJSON(value, output);
    auto* text = output.buffer ? output.buffer : output.string.impl;
    std::string result;
    if (text && output.length) {
        if (output.is8Bit) {
            auto* chars = output.buffer ? output.characters8 : static_cast<const unsigned char*>(text->data);
            result.assign(reinterpret_cast<const char*>(chars), static_cast<unsigned>(output.length));
        } else {
            auto* chars = output.buffer ? output.characters16 : static_cast<const char16_t*>(text->data);
            for (unsigned i = 0; i < static_cast<unsigned>(output.length); ++i) {
                Require(chars[i] <= 0x7f);
                result.push_back(static_cast<char>(chars[i]));
            }
        }
    }
    NKWebKit::WtfAbi::Release(output.buffer);
    NKWebKit::WtfAbi::Release(output.string.impl);
    return result;
}

static String Make8BitString(const unsigned char* data, unsigned length) {
    return NKWebKit::WtfAbi::String(_ZN3WTF10StringImpl6createEPKhj(data, length));
}

static String Make16BitString(const char16_t* data, unsigned length) {
    return NKWebKit::WtfAbi::String(_ZN3WTF10StringImpl6createEPKDsj(data, length));
}

static void TestEscapeBasic() {
    const unsigned char input1[] = {'a', '\b', 'b', '\f', 'c', '\n', 'd', '\r', 'e', '\t', 'f', '\\', 'g', '"', 'h'};
    String s1 = Make8BitString(input1, sizeof(input1));
    auto v1 = _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(s1);
    Require(Serialize(v1.Get()) == "\"a\\bb\\fc\\nd\\re\\tf\\\\g\\\"h\"");

    const unsigned char input2[] = {'<', '>', '&'};
    String s2 = Make8BitString(input2, sizeof(input2));
    auto v2 = _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(s2);
    Require(Serialize(v2.Get()) == "\"\\u003C\\u003E&\"");
}

static void TestEscapeControlChars() {
    const unsigned char input[] = {0x00, 0x01, 0x1b, 0x1f};
    String s = Make8BitString(input, sizeof(input));
    auto v = _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(s);
    Require(Serialize(v.Get()) == "\"\\u0000\\u0001\\u001B\\u001F\"");
}

static void TestEscapeDelAndHighAscii() {
    const unsigned char input[] = {0x7f, 0x80, 0xe9};
    String s = Make8BitString(input, sizeof(input));
    auto v = _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(s);
    Require(Serialize(v.Get()) == "\"\\u007F\\u0080\\u00E9\"");
}

static void TestEscapeEuroSign16Bit() {
    const char16_t input[] = {0x20ac};
    String s = Make16BitString(input, 1);
    auto v = _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(s);
    Require(Serialize(v.Get()) == "\"\\u20AC\"");
}

static void TestEscapeNonBMP() {
    const char16_t input[] = {0xD83D, 0xDE00};
    String s = Make16BitString(input, 2);
    auto v = _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(s);
    Require(Serialize(v.Get()) == "\"\\uD83D\\uDE00\"");
}

static void TestEscapeIsolatedSurrogates() {
    const char16_t input1[] = {0xD800};
    String s1 = Make16BitString(input1, 1);
    auto v1 = _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(s1);
    Require(Serialize(v1.Get()) == "\"\\uD800\"");

    const char16_t input2[] = {0xDC00};
    String s2 = Make16BitString(input2, 1);
    auto v2 = _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(s2);
    Require(Serialize(v2.Get()) == "\"\\uDC00\"");

    const char16_t input3[] = {0xD800, 0xDC00, 0xD83D, 0xDE00, 0xDC00, 0xD800};
    String s3 = Make16BitString(input3, 6);
    auto v3 = _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(s3);
    Require(Serialize(v3.Get()) == "\"\\uD800\\uDC00\\uD83D\\uDE00\\uDC00\\uD800\"");
}

static void Test8BitVs16BitStrings() {
    const unsigned char latin1[] = {'a', 0xe9, 'b'};
    String s8 = Make8BitString(latin1, sizeof(latin1));
    auto v8 = _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(s8);
    Require(Serialize(v8.Get()) == "\"a\\u00E9b\"");

    const char16_t utf16[] = {'a', 0x00e9, 'b'};
    String s16 = Make16BitString(utf16, 3);
    auto v16 = _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(s16);
    Require(Serialize(v16.Get()) == "\"a\\u00E9b\"");
}

static void TestNumberSerialization() {
    auto v0 = _ZN3WTF8JSONImpl5Value6createEi(0);
    Require(Serialize(v0.Get()) == "0");

    auto vNegZero = _ZN3WTF8JSONImpl5Value6createEd(-0.0);
    Require(Serialize(vNegZero.Get()) == "0");

    auto v1e20 = _ZN3WTF8JSONImpl5Value6createEd(1e20);
    Require(Serialize(v1e20.Get()) == "100000000000000000000");

    auto v1e21 = _ZN3WTF8JSONImpl5Value6createEd(1e21);
    Require(Serialize(v1e21.Get()) == "1e+21");

    auto vSubnormal = _ZN3WTF8JSONImpl5Value6createEd(5e-324);
    Require(Serialize(vSubnormal.Get()) == "5e-324");

    auto vDblMin = _ZN3WTF8JSONImpl5Value6createEd(DBL_MIN);
    std::string dblMinStr = Serialize(vDblMin.Get());
    char* minEnd = nullptr; Require(std::strtod(dblMinStr.c_str(), &minEnd) == DBL_MIN && minEnd == dblMinStr.c_str() + dblMinStr.size() && dblMinStr.find("e-308") != std::string::npos);

    auto vDblMax = _ZN3WTF8JSONImpl5Value6createEd(DBL_MAX);
    std::string dblMaxStr = Serialize(vDblMax.Get());
    char* maxEnd = nullptr; Require(std::strtod(dblMaxStr.c_str(), &maxEnd) == DBL_MAX && maxEnd == dblMaxStr.c_str() + dblMaxStr.size() && dblMaxStr.find("e+308") != std::string::npos);

    auto vNaN = _ZN3WTF8JSONImpl5Value6createEd(std::numeric_limits<double>::quiet_NaN());
    Require(Serialize(vNaN.Get()) == "null");

    auto vNegInf = _ZN3WTF8JSONImpl5Value6createEd(-std::numeric_limits<double>::infinity());
    Require(Serialize(vNegInf.Get()) == "null");
}

static void TestEmptyAndNestedArrays() {
    auto emptyArray = _ZN3WTF8JSONImpl5Array6createEv();
    Require(Serialize(emptyArray.Get()) == "[]");

    auto inner = _ZN3WTF8JSONImpl5Array6createEv();
    _ZN3WTF8JSONImpl9ArrayBase11pushIntegerEi(inner.Get(), 1);
    _ZN3WTF8JSONImpl9ArrayBase11pushIntegerEi(inner.Get(), 2);

    auto outer = _ZN3WTF8JSONImpl5Array6createEv();
    _ZN3WTF8JSONImpl9ArrayBase11pushIntegerEi(outer.Get(), 0);
    _ZN3WTF8JSONImpl9ArrayBase11pushIntegerEi(outer.Get(), 0);
    outer->elements.buffer[1] = NKWebKit::JsonAbi::Ref<Value>(inner);
    Require(Serialize(outer.Get()) == "[0,[1,2]]");

    auto deep = _ZN3WTF8JSONImpl5Array6createEv();
    auto level2 = _ZN3WTF8JSONImpl5Array6createEv();
    auto level3 = _ZN3WTF8JSONImpl5Array6createEv();
    _ZN3WTF8JSONImpl9ArrayBase11pushIntegerEi(level3.Get(), 42);
    _ZN3WTF8JSONImpl9ArrayBase11pushIntegerEi(level2.Get(), 0);
    level2->elements.buffer[0] = NKWebKit::JsonAbi::Ref<Value>(level3);
    _ZN3WTF8JSONImpl9ArrayBase11pushIntegerEi(deep.Get(), 0);
    deep->elements.buffer[0] = NKWebKit::JsonAbi::Ref<Value>(level2);
    Require(Serialize(deep.Get()) == "[[[42]]]");
}

static void TestAsObjectAsArrayOnWrongTypes() {
    auto vNull = _ZN3WTF8JSONImpl5Value4nullEv();
    Require(vNull->vptr->asObject(vNull.Get()).Get() == nullptr);
    Require(vNull->vptr->asArray(vNull.Get()).Get() == nullptr);

    auto vBool = _ZN3WTF8JSONImpl5Value6createEb(true);
    Require(vBool->vptr->asObject(vBool.Get()).Get() == nullptr);
    Require(vBool->vptr->asArray(vBool.Get()).Get() == nullptr);

    auto vInt = _ZN3WTF8JSONImpl5Value6createEi(42);
    Require(vInt->vptr->asObject(vInt.Get()).Get() == nullptr);
    Require(vInt->vptr->asArray(vInt.Get()).Get() == nullptr);

    auto vDbl = _ZN3WTF8JSONImpl5Value6createEd(3.14);
    Require(vDbl->vptr->asObject(vDbl.Get()).Get() == nullptr);
    Require(vDbl->vptr->asArray(vDbl.Get()).Get() == nullptr);

    String str = Make8BitString(reinterpret_cast<const unsigned char*>("test"), 4);
    auto vStr = _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(str);
    Require(vStr->vptr->asObject(vStr.Get()).Get() == nullptr);
    Require(vStr->vptr->asArray(vStr.Get()).Get() == nullptr);

    auto vArr = _ZN3WTF8JSONImpl5Array6createEv();
    Require(vArr->vptr->asObject(vArr.Get()).Get() == nullptr);
    Require(vArr->vptr->asArray(vArr.Get()).Get() != nullptr);

    auto vObj = _ZN3WTF8JSONImpl6Object6createEv();
    Require(_ZNK3WTF8JSONImpl5Value4typeEv(vObj.Get()) == ValueType::Object);
    Require(vObj->vptr->asObject(vObj.Get()).Get() != nullptr);
    Require(vObj->vptr->asArray(vObj.Get()).Get() == nullptr);
}

static void TestAsObjectAsArrayOnCorrectTypes() {
    auto obj = _ZN3WTF8JSONImpl6Object6createEv();
    Require(_ZNK3WTF8JSONImpl5Value4typeEv(obj.Get()) == ValueType::Object);
    auto asObject = obj->vptr->asObject(obj.Get());
    Require(asObject.Get() == obj.Get());
    Require(obj->vptr->asArray(obj.Get()).Get() == nullptr);

    auto arr = _ZN3WTF8JSONImpl5Array6createEv();
    auto asArray = arr->vptr->asArray(arr.Get());
    Require(asArray.Get() == arr.Get());
    Require(arr->vptr->asObject(arr.Get()).Get() == nullptr);
}

static void TestArrayWithMixedTypes() {
    auto arr = _ZN3WTF8JSONImpl5Array6createEv();
    _ZN3WTF8JSONImpl9ArrayBase11pushBooleanEb(arr.Get(), true);
    _ZN3WTF8JSONImpl9ArrayBase11pushIntegerEi(arr.Get(), -42);
    _ZN3WTF8JSONImpl9ArrayBase10pushDoubleEd(arr.Get(), 3.14);
    String str = Make8BitString(reinterpret_cast<const unsigned char*>("\"\\"), 2);
    _ZN3WTF8JSONImpl9ArrayBase10pushStringERKNS_6StringE(arr.Get(), str);
    _ZN3WTF8JSONImpl9ArrayBase11pushIntegerEi(arr.Get(), 0);
    auto nested = _ZN3WTF8JSONImpl5Array6createEv();
    _ZN3WTF8JSONImpl9ArrayBase11pushIntegerEi(nested.Get(), 99);
    arr->elements.buffer[4] = NKWebKit::JsonAbi::Ref<Value>(nested);
    Require(Serialize(arr.Get()) == "[true,-42,3.14,\"\\\"\\\\\",[99]]");
}

static void TestStringWithNewlinesAndQuotesInArray() {
    const unsigned char input[] = {'h', 'e', 'l', 'l', 'o', '\n', 'w', 'o', 'r', 'l', 'd', '\r', '\t', '"', '\\', '<', '>'};
    String s = Make8BitString(input, sizeof(input));
    auto arr = _ZN3WTF8JSONImpl5Array6createEv();
    _ZN3WTF8JSONImpl9ArrayBase10pushStringERKNS_6StringE(arr.Get(), s);
    Require(Serialize(arr.Get()) == "[\"hello\\nworld\\r\\t\\\"\\\\\\u003C\\u003E\"]");
}

int main() {
    TestEscapeBasic();
    TestEscapeControlChars();
    TestEscapeDelAndHighAscii();
    TestEscapeEuroSign16Bit();
    TestEscapeNonBMP();
    TestEscapeIsolatedSurrogates();
    Test8BitVs16BitStrings();
    TestNumberSerialization();
    TestEmptyAndNestedArrays();
    TestAsObjectAsArrayOnWrongTypes();
    TestAsObjectAsArrayOnCorrectTypes();
    TestArrayWithMixedTypes();
    TestStringWithNewlinesAndQuotesInArray();
    return 0;
}
