#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::WebCoreAbi::AbsoluteCoordinates;
using NKWebKit::WebCoreAbi::PathCoordinateMode;
using NKWebKit::WebCoreAbi::RelativeCoordinates;
using NKWebKit::WebCoreAbi::SVGPathStringBuilder;
using NKWebKit::WtfAbi::String;
using WebCore::FloatPoint;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "SVGPathStringBuilder check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

using IncrementSlot = void (APS5_VABI*)(SVGPathStringBuilder*);
using ContinueSlot = bool (APS5_VABI*)(SVGPathStringBuilder*);
using ScalarSlot = void (APS5_VABI*)(SVGPathStringBuilder*, float, PathCoordinateMode);
using SmoothCubicSlot = void (APS5_VABI*)(SVGPathStringBuilder*, const FloatPoint*, const FloatPoint*, PathCoordinateMode);
using SmoothQuadraticSlot = void (APS5_VABI*)(SVGPathStringBuilder*, const FloatPoint*, PathCoordinateMode);
using ArcSlot = void (APS5_VABI*)(SVGPathStringBuilder*, float, float, float, bool, bool, const FloatPoint*, PathCoordinateMode);
using DestroySlot = void (APS5_VABI*)(SVGPathStringBuilder*);

template<typename T> T Slot(const SVGPathStringBuilder& builder, int index) {
    return reinterpret_cast<T>(const_cast<void*>(builder.vtable[index]));
}

float Float(unsigned bits) {
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::string Text(const String& string) {
    Require(string.impl && (string.impl->hashAndFlags & 4));
    return std::string(static_cast<const char*>(string.impl->data), string.impl->length);
}

struct Builder {
    SVGPathStringBuilder object;
    Builder() {
        std::memset(&object, 0xA5, sizeof(object));
        _ZN7WebCore20SVGPathStringBuilderC1Ev(&object);
    }
    ~Builder() { _ZN7WebCore20SVGPathStringBuilderD1Ev(&object); }
    SVGPathStringBuilder* get() { return &object; }
};

void AppendAll(Builder& builder, PathCoordinateMode mode, const FloatPoint (&p)[12], float h, float v, float r1, float r2, float angle, bool largeArc, bool sweep) {
    auto* self = builder.get();
    _ZN7WebCore20SVGPathStringBuilder6moveToERKNS_10FloatPointEbNS_18PathCoordinateModeE(self, &p[0], true, mode);
    _ZN7WebCore20SVGPathStringBuilder6lineToERKNS_10FloatPointENS_18PathCoordinateModeE(self, &p[1], mode);
    _ZN7WebCore20SVGPathStringBuilder12curveToCubicERKNS_10FloatPointES3_S3_NS_18PathCoordinateModeE(self, &p[2], &p[3], &p[4], mode);
    _ZN7WebCore20SVGPathStringBuilder16curveToQuadraticERKNS_10FloatPointES3_NS_18PathCoordinateModeE(self, &p[5], &p[6], mode);
    Slot<ScalarSlot>(builder.object, 6)(self, h, mode);
    Slot<ScalarSlot>(builder.object, 7)(self, v, mode);
    Slot<SmoothCubicSlot>(builder.object, 8)(self, &p[7], &p[8], mode);
    Slot<SmoothQuadraticSlot>(builder.object, 10)(self, &p[9], mode);
    Slot<ArcSlot>(builder.object, 11)(self, r1, r2, angle, largeArc, sweep, &p[10], mode);
    Slot<DestroySlot>(builder.object, 5)(self);
}

void TestConstruction() {
    Builder builder;
    const auto& b = builder.object.stringBuilder;
    Require(!b.string.impl && !b.buffer && !b.characters8 && !b.overflowed && b.shouldCrashOnOverflow && b.length == 0 && b.is8Bit);
    const void* const* table = builder.object.vtable - 2;
    Require(!table[0] && !table[1]);
    for (int slot = 2; slot < 16; ++slot) Require(table[slot] != nullptr);
#ifndef _WIN32
    Require(table[4] == reinterpret_cast<const void*>(&_ZN7WebCore20SVGPathStringBuilder6moveToERKNS_10FloatPointEbNS_18PathCoordinateModeE));
    Require(table[5] == reinterpret_cast<const void*>(&_ZN7WebCore20SVGPathStringBuilder6lineToERKNS_10FloatPointENS_18PathCoordinateModeE));
    Require(table[6] == reinterpret_cast<const void*>(&_ZN7WebCore20SVGPathStringBuilder12curveToCubicERKNS_10FloatPointES3_S3_NS_18PathCoordinateModeE));
    Require(table[7] == reinterpret_cast<const void*>(&_ZN7WebCore20SVGPathStringBuilder9closePathEv));
    Require(table[11] == reinterpret_cast<const void*>(&_ZN7WebCore20SVGPathStringBuilder16curveToQuadraticERKNS_10FloatPointES3_NS_18PathCoordinateModeE));
    Require(table[14] == reinterpret_cast<const void*>(&_ZN7WebCore20SVGPathStringBuilderD1Ev));
    Require(table[15] == reinterpret_cast<const void*>(&_ZN7WebCore20SVGPathStringBuilderD0Ev));
#endif
    Slot<IncrementSlot>(builder.object, 0)(builder.get());
    Require(Slot<ContinueSlot>(builder.object, 1)(builder.get()));
    Require(builder.object.stringBuilder.length == 0);

    SVGPathStringBuilder base;
    std::memset(&base, 0x5A, sizeof(base));
    _ZN7WebCore20SVGPathStringBuilderC2Ev(&base);
    Require(base.vtable == builder.object.vtable && base.stringBuilder.length == 0);
    _ZN7WebCore20SVGPathStringBuilderD2Ev(&base);
}

void TestEmptyResult() {
    Builder builder;
    String result = _ZN7WebCore20SVGPathStringBuilder6resultEv(builder.get());
    Require(!result.impl);
}

void TestCommands() {
    {
        Builder builder;
        const FloatPoint points[12] = {{1, 2}, {3.5f, -4.25f}, {1, 2}, {3, 4}, {5, 6}, {0.1f, 0.2f}, {0.3f, 0.4f}, {1, 1}, {2, 2}, {9, 10}, {50, 25}, {}};
        AppendAll(builder, AbsoluteCoordinates, points, 7, 8, 25, 26, -30, true, false);
        String first = _ZN7WebCore20SVGPathStringBuilder6resultEv(builder.get());
        Require(Text(first) == "M 1 2 L 3.5 -4.25 C 1 2 3 4 5 6 Q 0.1 0.2 0.3 0.4 H 7 V 8 S 1 1 2 2 T 9 10 A 25 26 -30 1 0 50 25 Z");
        String second = _ZN7WebCore20SVGPathStringBuilder6resultEv(builder.get());
        Require(Text(second) == "M 1 2 L 3.5 -4.25 C 1 2 3 4 5 6 Q 0.1 0.2 0.3 0.4 H 7 V 8 S 1 1 2 2 T 9 10 A 25 26 -30 1 0 50 25 ");
    }
    {
        Builder builder;
        const FloatPoint points[12] = {{-1, -2}, {0.5f, 0.25f}, {10, 20}, {30, 40}, {50, 60}, {1e-7f, 2e7f}, {0, -0.0f}, {3, 4}, {5, 6}, {-9, -10}, {8, 9}, {}};
        AppendAll(builder, RelativeCoordinates, points, -7, -8, 5, 6, 7, false, true);
        String first = _ZN7WebCore20SVGPathStringBuilder6resultEv(builder.get());
        Require(Text(first) == "m -1 -2 l 0.5 0.25 c 10 20 30 40 50 60 q 1e-7 2e+7 0 0 h -7 v -8 s 3 4 5 6 t -9 -10 a 5 6 7 0 1 8 9 Z");
        String second = _ZN7WebCore20SVGPathStringBuilder6resultEv(builder.get());
        Require(Text(second) == "m -1 -2 l 0.5 0.25 c 10 20 30 40 50 60 q 1e-7 2e+7 0 0 h -7 v -8 s 3 4 5 6 t -9 -10 a 5 6 7 0 1 8 9 ");
    }
}

struct NumberCase {
    unsigned bits;
    const char* text;
};

const NumberCase NumberCases[] = {
    {0x00000000u, "0"},
    {0x80000000u, "0"},
    {0x3f800000u, "1"},
    {0xbf800000u, "-1"},
    {0x3dcccccdu, "0.1"},
    {0x3f000000u, "0.5"},
    {0x3e99999au, "0.3"},
    {0x358637bdu, "0.000001"},
    {0x33d6bf95u, "1e-7"},
    {0x497423f8u, "1e+6"},
    {0x497423f6u, "999999"},
    {0x49742400u, "1e+6"},
    {0x47f12040u, "123457"},
    {0x4996b428u, "1.23457e+6"},
    {0x7f7fffffu, "3.40282e+38"},
    {0x00000001u, "1.4013e-45"},
    {0x7f800000u, "Infinity"},
    {0xff800000u, "-Infinity"},
    {0x7fc00000u, "NaN"},
    {0x35c9539cu, "0.0000015"},
    {0x4640e69au, "12345.7"},
    {0x3f800004u, "1"},
    {0x47c34ffau, "100000"},
    {0xc3889333u, "-273.15"},
    {0x37d1b717u, "0.000025"}
};

void TestNumbers() {
    for (const auto& item : NumberCases) {
        Builder builder;
        Slot<ScalarSlot>(builder.object, 6)(builder.get(), Float(item.bits), AbsoluteCoordinates);
        String result = _ZN7WebCore20SVGPathStringBuilder6resultEv(builder.get());
        Require(Text(result) == std::string("H ") + item.text);
    }
}

void TestDeletingDestructor() {
    auto* object = static_cast<SVGPathStringBuilder*>(_ZN3WTF10fastMallocEm(sizeof(SVGPathStringBuilder)));
    _ZN7WebCore20SVGPathStringBuilderC1Ev(object);
    const FloatPoint point{1, 2};
    for (int i = 0; i < 40; ++i) _ZN7WebCore20SVGPathStringBuilder6lineToERKNS_10FloatPointENS_18PathCoordinateModeE(object, &point, AbsoluteCoordinates);
    Require(object->stringBuilder.buffer != nullptr && object->stringBuilder.length == 40 * 6);
    Slot<DestroySlot>(*object, 13)(object);
}

void TestNull() {
    const FloatPoint point{};
    RequireInvalidArgument([] { _ZN7WebCore20SVGPathStringBuilderC1Ev(nullptr); });
    RequireInvalidArgument([] { _ZN7WebCore20SVGPathStringBuilderD1Ev(nullptr); });
    RequireInvalidArgument([] { _ZN7WebCore20SVGPathStringBuilder6resultEv(nullptr); });
    RequireInvalidArgument([&] { _ZN7WebCore20SVGPathStringBuilder6lineToERKNS_10FloatPointENS_18PathCoordinateModeE(nullptr, &point, AbsoluteCoordinates); });
    Builder builder;
    RequireInvalidArgument([&] { _ZN7WebCore20SVGPathStringBuilder6moveToERKNS_10FloatPointEbNS_18PathCoordinateModeE(builder.get(), nullptr, false, AbsoluteCoordinates); });
    RequireInvalidArgument([&] { _ZN7WebCore20SVGPathStringBuilder12curveToCubicERKNS_10FloatPointES3_S3_NS_18PathCoordinateModeE(builder.get(), &point, nullptr, &point, AbsoluteCoordinates); });
    RequireInvalidArgument([&] { Slot<SmoothQuadraticSlot>(builder.object, 10)(builder.get(), nullptr, AbsoluteCoordinates); });
    Require(builder.object.stringBuilder.length == 0);
}

}  // namespace

int main() {
    TestConstruction();
    TestEmptyResult();
    TestCommands();
    TestNumbers();
    TestDeletingDestructor();
    TestNull();
    std::puts("SVGPathStringBuilder passed");
    return 0;
}
