#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>

static void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "Check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(cond) Check((cond), __LINE__)

using NKWebKit::JsonAbi::Array;
using NKWebKit::JsonAbi::ArrayBase;
using NKWebKit::JsonAbi::Value;
using NKWebKit::JsonAbi::ValueType;
using NKWebKit::WtfAbi::String;

static void TestValueNull() {
    auto v = _ZN3WTF8JSONImpl5Value4nullEv();
    Require(v.Get() != nullptr);
    Require(_ZNK3WTF8JSONImpl5Value4typeEv(v.Get()) == ValueType::Null);
    Require(_ZNK3WTF8JSONImpl5Value6isNullEv(v.Get()));

    double outDouble = 0.0;
    Require(!_ZNK3WTF8JSONImpl5Value8asDoubleERd(v.Get(), outDouble));

    int outInt = 0;
    Require(!_ZNK3WTF8JSONImpl5Value9asIntegerERi(v.Get(), outInt));

    Require(_ZNK3WTF8JSONImpl5Value10memoryCostEv(v.Get()) == sizeof(Value*));
}

static void TestValueBoolean() {
    auto bTrue = _ZN3WTF8JSONImpl5Value6createEb(true);
    Require(bTrue.Get() != nullptr);
    Require(_ZNK3WTF8JSONImpl5Value4typeEv(bTrue.Get()) == ValueType::Boolean);
    Require(!_ZNK3WTF8JSONImpl5Value6isNullEv(bTrue.Get()));

    double outDouble = 0.0;
    Require(!_ZNK3WTF8JSONImpl5Value8asDoubleERd(bTrue.Get(), outDouble));

    int outInt = 0;
    Require(!_ZNK3WTF8JSONImpl5Value9asIntegerERi(bTrue.Get(), outInt));

    auto bFalse = _ZN3WTF8JSONImpl5Value6createEb(false);
    Require(bFalse.Get() != nullptr);
    Require(_ZNK3WTF8JSONImpl5Value4typeEv(bFalse.Get()) == ValueType::Boolean);
    Require(!_ZNK3WTF8JSONImpl5Value6isNullEv(bFalse.Get()));
}

static void TestValueInteger() {
    auto v = _ZN3WTF8JSONImpl5Value6createEi(42);
    Require(v.Get() != nullptr);
    Require(_ZNK3WTF8JSONImpl5Value4typeEv(v.Get()) == ValueType::Integer);
    Require(!_ZNK3WTF8JSONImpl5Value6isNullEv(v.Get()));

    int outInt = 0;
    Require(_ZNK3WTF8JSONImpl5Value9asIntegerERi(v.Get(), outInt));
    Require(outInt == 42);

    double outDouble = 0.0;
    Require(_ZNK3WTF8JSONImpl5Value8asDoubleERd(v.Get(), outDouble));
    Require(outDouble == 42.0);

    auto vNeg = _ZN3WTF8JSONImpl5Value6createEi(-100);
    outInt = 0;
    Require(_ZNK3WTF8JSONImpl5Value9asIntegerERi(vNeg.Get(), outInt));
    Require(outInt == -100);

    auto vZero = _ZN3WTF8JSONImpl5Value6createEi(0);
    outInt = 1;
    Require(_ZNK3WTF8JSONImpl5Value9asIntegerERi(vZero.Get(), outInt));
    Require(outInt == 0);
}

static void TestValueDouble() {
    auto v = _ZN3WTF8JSONImpl5Value6createEd(3.14);
    Require(v.Get() != nullptr);
    Require(_ZNK3WTF8JSONImpl5Value4typeEv(v.Get()) == ValueType::Double);
    Require(!_ZNK3WTF8JSONImpl5Value6isNullEv(v.Get()));

    double outDouble = 0.0;
    Require(_ZNK3WTF8JSONImpl5Value8asDoubleERd(v.Get(), outDouble));
    Require(outDouble == 3.14);

    int outInt = 0;
    Require(_ZNK3WTF8JSONImpl5Value9asIntegerERi(v.Get(), outInt));
    Require(outInt == 3);

    auto vNeg = _ZN3WTF8JSONImpl5Value6createEd(-2.718);
    outDouble = 0.0;
    Require(_ZNK3WTF8JSONImpl5Value8asDoubleERd(vNeg.Get(), outDouble));
    Require(outDouble == -2.718);
}

static void TestValueString() {
    String str = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>("test"), 4U);
    auto v = _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(str);
    Require(v.Get() != nullptr);
    Require(_ZNK3WTF8JSONImpl5Value4typeEv(v.Get()) == ValueType::String);
    Require(!_ZNK3WTF8JSONImpl5Value6isNullEv(v.Get()));

    double outDouble = 0.0;
    Require(!_ZNK3WTF8JSONImpl5Value8asDoubleERd(v.Get(), outDouble));

    int outInt = 0;
    Require(!_ZNK3WTF8JSONImpl5Value9asIntegerERi(v.Get(), outInt));

    Require(_ZNK3WTF8JSONImpl5Value10memoryCostEv(v.Get()) == sizeof(Value*) + sizeof(NKWebKit::WtfAbi::StringImpl) + 4U);

    String emptyStr = _ZN3WTF10StringImpl6createEPKhj(nullptr, 0U);
    auto vEmpty = _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(emptyStr);
    Require(_ZNK3WTF8JSONImpl5Value10memoryCostEv(vEmpty.Get()) == sizeof(Value*) + sizeof(NKWebKit::WtfAbi::StringImpl));

    String nullStr;
    nullStr.impl = nullptr;
    auto vNullStr = _ZN3WTF8JSONImpl5Value6createERKNS_6StringE(nullStr);
    Require(_ZNK3WTF8JSONImpl5Value10memoryCostEv(vNullStr.Get()) == sizeof(Value*));
}

static void TestValueConstructorsAndDestructors() {
    alignas(alignof(Value)) unsigned char buf[sizeof(Value)];
    auto* p = reinterpret_cast<Value*>(buf);

    _ZN3WTF8JSONImpl5ValueC1Ev(p);
    Require(_ZNK3WTF8JSONImpl5Value6isNullEv(p));
    _ZN3WTF8JSONImpl5ValueD2Ev(p);

    _ZN3WTF8JSONImpl5ValueC2Ev(p);
    Require(_ZNK3WTF8JSONImpl5Value6isNullEv(p));
    _ZN3WTF8JSONImpl5ValueD1Ev(p);

    _ZN3WTF8JSONImpl5ValueC1ENS1_4TypeE(p, ValueType::Double);
    Require(_ZNK3WTF8JSONImpl5Value4typeEv(p) == ValueType::Double);
    _ZN3WTF8JSONImpl5ValueD2Ev(p);

    _ZN3WTF8JSONImpl5ValueC2ENS1_4TypeE(p, ValueType::Integer);
    Require(_ZNK3WTF8JSONImpl5Value4typeEv(p) == ValueType::Integer);
    _ZN3WTF8JSONImpl5ValueD1Ev(p);

    _ZN3WTF8JSONImpl5ValueC1Eb(p, true);
    Require(_ZNK3WTF8JSONImpl5Value4typeEv(p) == ValueType::Boolean);
    _ZN3WTF8JSONImpl5ValueD2Ev(p);

    _ZN3WTF8JSONImpl5ValueC2Eb(p, false);
    Require(_ZNK3WTF8JSONImpl5Value4typeEv(p) == ValueType::Boolean);
    _ZN3WTF8JSONImpl5ValueD1Ev(p);

    _ZN3WTF8JSONImpl5ValueC1Ei(p, 123);
    Require(_ZNK3WTF8JSONImpl5Value4typeEv(p) == ValueType::Integer);
    int valI = 0;
    Require(_ZNK3WTF8JSONImpl5Value9asIntegerERi(p, valI));
    Require(valI == 123);
    _ZN3WTF8JSONImpl5ValueD2Ev(p);

    _ZN3WTF8JSONImpl5ValueC2Ei(p, -456);
    Require(_ZNK3WTF8JSONImpl5Value4typeEv(p) == ValueType::Integer);
    Require(_ZNK3WTF8JSONImpl5Value9asIntegerERi(p, valI));
    Require(valI == -456);
    _ZN3WTF8JSONImpl5ValueD1Ev(p);

    _ZN3WTF8JSONImpl5ValueC1Ed(p, 1.5);
    Require(_ZNK3WTF8JSONImpl5Value4typeEv(p) == ValueType::Double);
    double valD = 0.0;
    Require(_ZNK3WTF8JSONImpl5Value8asDoubleERd(p, valD));
    Require(valD == 1.5);
    _ZN3WTF8JSONImpl5ValueD2Ev(p);

    _ZN3WTF8JSONImpl5ValueC2Ed(p, -2.5);
    Require(_ZNK3WTF8JSONImpl5Value4typeEv(p) == ValueType::Double);
    Require(_ZNK3WTF8JSONImpl5Value8asDoubleERd(p, valD));
    Require(valD == -2.5);
    _ZN3WTF8JSONImpl5ValueD1Ev(p);

    String str = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>("cstr"), 4U);
    _ZN3WTF8JSONImpl5ValueC1ERKNS_6StringE(p, str);
    Require(_ZNK3WTF8JSONImpl5Value4typeEv(p) == ValueType::String);
    _ZN3WTF8JSONImpl5ValueD2Ev(p);

    _ZN3WTF8JSONImpl5ValueC2ERKNS_6StringE(p, str);
    Require(_ZNK3WTF8JSONImpl5Value4typeEv(p) == ValueType::String);
    _ZN3WTF8JSONImpl5ValueD1Ev(p);

    auto* heapVal = static_cast<Value*>(_ZN3WTF10fastMallocEm(sizeof(Value)));
    _ZN3WTF8JSONImpl5ValueC1Ev(heapVal);
    _ZN3WTF8JSONImpl5ValueD0Ev(heapVal);
}

static void TestArrayCreateAndPush() {
    auto arr = _ZN3WTF8JSONImpl5Array6createEv();
    Require(arr.Get() != nullptr);
    Require(_ZNK3WTF8JSONImpl5Value4typeEv(arr.Get()) == ValueType::Array);
    Require(!_ZNK3WTF8JSONImpl5Value6isNullEv(arr.Get()));
    Require(_ZN3WTF8JSONImpl9ArrayBase5beginEv(arr.Get()) == _ZN3WTF8JSONImpl9ArrayBase3endEv(arr.Get()));
    Require(_ZNK3WTF8JSONImpl9ArrayBase5beginEv(arr.Get()) == nullptr);
    Require(_ZNK3WTF8JSONImpl9ArrayBase10memoryCostEv(arr.Get()) == sizeof(Value*));

    bool threw = false;
    try {
        _ZNK3WTF8JSONImpl9ArrayBase3getEm(arr.Get(), 0);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    Require(threw);

    String str = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>("item"), 4U);

    _ZN3WTF8JSONImpl9ArrayBase11pushBooleanEb(arr.Get(), true);
    _ZN3WTF8JSONImpl9ArrayBase11pushIntegerEi(arr.Get(), 99);
    _ZN3WTF8JSONImpl9ArrayBase10pushDoubleEd(arr.Get(), 4.5);
    _ZN3WTF8JSONImpl9ArrayBase10pushStringERKNS_6StringE(arr.Get(), str);

    Require(_ZN3WTF8JSONImpl9ArrayBase3endEv(arr.Get()) - _ZN3WTF8JSONImpl9ArrayBase5beginEv(arr.Get()) == 4);
    Require(_ZNK3WTF8JSONImpl9ArrayBase5beginEv(arr.Get()) == _ZN3WTF8JSONImpl9ArrayBase5beginEv(arr.Get()));

    auto e0 = _ZNK3WTF8JSONImpl9ArrayBase3getEm(arr.Get(), 0);
    Require(_ZNK3WTF8JSONImpl5Value4typeEv(e0.Get()) == ValueType::Boolean);

    auto e1 = _ZNK3WTF8JSONImpl9ArrayBase3getEm(arr.Get(), 1);
    Require(_ZNK3WTF8JSONImpl5Value4typeEv(e1.Get()) == ValueType::Integer);
    int intVal = 0;
    Require(_ZNK3WTF8JSONImpl5Value9asIntegerERi(e1.Get(), intVal));
    Require(intVal == 99);

    auto e2 = _ZNK3WTF8JSONImpl9ArrayBase3getEm(arr.Get(), 2);
    Require(_ZNK3WTF8JSONImpl5Value4typeEv(e2.Get()) == ValueType::Double);
    double dblVal = 0.0;
    Require(_ZNK3WTF8JSONImpl5Value8asDoubleERd(e2.Get(), dblVal));
    Require(dblVal == 4.5);

    auto e3 = _ZNK3WTF8JSONImpl9ArrayBase3getEm(arr.Get(), 3);
    Require(_ZNK3WTF8JSONImpl5Value4typeEv(e3.Get()) == ValueType::String);

    threw = false;
    try {
        _ZNK3WTF8JSONImpl9ArrayBase3getEm(arr.Get(), 4);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    Require(threw);

    std::size_t expectedCost = sizeof(Value*) + sizeof(Value*) + sizeof(Value*) + sizeof(Value*) + (sizeof(Value*) + sizeof(NKWebKit::WtfAbi::StringImpl) + 4U);
    Require(_ZNK3WTF8JSONImpl9ArrayBase10memoryCostEv(arr.Get()) == expectedCost);
}

static void TestArrayConstructorsAndDestructors() {
    alignas(alignof(ArrayBase)) unsigned char buf[sizeof(ArrayBase)];
    auto* p = reinterpret_cast<ArrayBase*>(buf);

    _ZN3WTF8JSONImpl9ArrayBaseC1Ev(p);
    Require(_ZNK3WTF8JSONImpl5Value4typeEv(p) == ValueType::Array);
    Require(_ZN3WTF8JSONImpl9ArrayBase5beginEv(p) == _ZN3WTF8JSONImpl9ArrayBase3endEv(p));
    _ZN3WTF8JSONImpl9ArrayBaseD1Ev(p);

    _ZN3WTF8JSONImpl9ArrayBaseC2Ev(p);
    Require(_ZNK3WTF8JSONImpl5Value4typeEv(p) == ValueType::Array);
    _ZN3WTF8JSONImpl9ArrayBaseD1Ev(p);

    auto* heapArr = static_cast<ArrayBase*>(_ZN3WTF10fastMallocEm(sizeof(ArrayBase)));
    _ZN3WTF8JSONImpl9ArrayBaseC1Ev(heapArr);
    _ZN3WTF8JSONImpl9ArrayBaseD0Ev(heapArr);
}

static void TestNullPointerExceptions() {
    bool threw = false;
    try {
        _ZNK3WTF8JSONImpl5Value4typeEv(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        _ZNK3WTF8JSONImpl5Value6isNullEv(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        double d = 0;
        _ZNK3WTF8JSONImpl5Value8asDoubleERd(nullptr, d);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        int i = 0;
        _ZNK3WTF8JSONImpl5Value9asIntegerERi(nullptr, i);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        _ZNK3WTF8JSONImpl5Value10memoryCostEv(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        _ZN3WTF8JSONImpl9ArrayBase11pushBooleanEb(nullptr, true);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        _ZNK3WTF8JSONImpl9ArrayBase3getEm(nullptr, 0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

int main() {
    TestValueNull();
    TestValueBoolean();
    TestValueInteger();
    TestValueDouble();
    TestValueString();
    TestValueConstructorsAndDestructors();
    TestArrayCreateAndPush();
    TestArrayConstructorsAndDestructors();
    TestNullPointerExceptions();
    return 0;
}
