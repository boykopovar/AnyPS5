#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>

using NKWebKit::WtfAbi::String;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore layout check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static String Narrow(const std::string& text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
}

template<typename Function> static bool Throws(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static void TestGridPosition() {
    WebCore::GridPosition grid;
    grid.m_type = WebCore::AutoPosition;
    grid.m_integerPosition = 0;

    String line = Narrow("a");
    unsigned before = line.impl->refCount;
    _ZN7WebCore12GridPosition19setExplicitPositionEiRKN3WTF6StringE(&grid, 5, line);
    Require(grid.m_type == WebCore::ExplicitPosition && grid.m_integerPosition == 5);
    Require(grid.m_namedGridLine.impl == line.impl && line.impl->refCount == before + 2);
    Require(_ZNK7WebCore12GridPosition15integerPositionEv(&grid) == 5);

    String other = Narrow("b");
    _ZN7WebCore12GridPosition15setSpanPositionEiRKN3WTF6StringE(&grid, -3, other);
    Require(grid.m_type == WebCore::SpanPosition && grid.m_integerPosition == -3);
    Require(grid.m_namedGridLine.impl == other.impl && line.impl->refCount == before);
    Require(_ZNK7WebCore12GridPosition12spanPositionEv(&grid) == -3);

    _ZN7WebCore12GridPosition19setExplicitPositionEiRKN3WTF6StringE(&grid, 2000000, String());
    Require(grid.m_integerPosition == 1000000 && grid.m_namedGridLine.impl == nullptr);
    _ZN7WebCore12GridPosition19setExplicitPositionEiRKN3WTF6StringE(&grid, -2000000, other);
    Require(grid.m_integerPosition == -1000000);
    _ZN7WebCore12GridPosition19setExplicitPositionEiRKN3WTF6StringE(&grid, 1000000, other);
    Require(grid.m_integerPosition == 1000000);
    _ZN7WebCore12GridPosition15setSpanPositionEiRKN3WTF6StringE(&grid, 999999, other);
    Require(grid.m_integerPosition == 999999);
    _ZN7WebCore12GridPosition15setSpanPositionEiRKN3WTF6StringE(&grid, 0, other);
    Require(grid.m_integerPosition == 0);
    _ZN7WebCore12GridPosition15setSpanPositionEiRKN3WTF6StringE(&grid, std::numeric_limits<int>::min(), other);
    Require(grid.m_integerPosition == -1000000);
    _ZN7WebCore12GridPosition19setExplicitPositionEiRKN3WTF6StringE(&grid, std::numeric_limits<int>::max(), other);
    Require(grid.m_integerPosition == 1000000);

    _ZN7WebCore12GridPosition24setMaxPositionForTestingEj(10);
    _ZN7WebCore12GridPosition19setExplicitPositionEiRKN3WTF6StringE(&grid, 15, other);
    Require(grid.m_integerPosition == 10);
    _ZN7WebCore12GridPosition19setExplicitPositionEiRKN3WTF6StringE(&grid, -15, other);
    Require(grid.m_integerPosition == -10);
    _ZN7WebCore12GridPosition19setExplicitPositionEiRKN3WTF6StringE(&grid, 10, other);
    Require(grid.m_integerPosition == 10);
    _ZN7WebCore12GridPosition15setSpanPositionEiRKN3WTF6StringE(&grid, 9, other);
    Require(grid.m_integerPosition == 9);
    _ZN7WebCore12GridPosition24setMaxPositionForTestingEj(1000000);
    _ZN7WebCore12GridPosition19setExplicitPositionEiRKN3WTF6StringE(&grid, 5000000, other);
    Require(grid.m_integerPosition == 1000000);

    Require(Throws([&] { _ZN7WebCore12GridPosition15setSpanPositionEiRKN3WTF6StringE(nullptr, 1, other); }));
}

static WebCore::Length IntLength(int value, WebCore::LengthType type) {
    WebCore::Length length{};
    length.m_intValue = value;
    length.m_type = type;
    length.m_hasQuirk = false;
    length.m_isFloat = false;
    return length;
}

static WebCore::Length FloatLength(float value, WebCore::LengthType type) {
    WebCore::Length length{};
    length.m_floatValue = value;
    length.m_type = type;
    length.m_hasQuirk = false;
    length.m_isFloat = true;
    return length;
}

static int ValueForLength(const WebCore::Length& length, int rawMaximum) {
    return _ZN7WebCore14valueForLengthERKNS_6LengthENS_10LayoutUnitE(length, WebCore::LayoutUnit{rawMaximum}).m_value;
}

static void TestLengthFunctions() {
    using WebCore::LengthType;
    Require(_ZN7WebCore19floatValueForLengthERKNS_6LengthEf(IntLength(10, LengthType::Fixed), 200.0f) == 10.0f);
    Require(_ZN7WebCore19floatValueForLengthERKNS_6LengthEf(FloatLength(12.5f, LengthType::Fixed), 200.0f) == 12.5f);
    Require(_ZN7WebCore19floatValueForLengthERKNS_6LengthEf(IntLength(50, LengthType::Percent), 200.0f) == 100.0f);
    Require(_ZN7WebCore19floatValueForLengthERKNS_6LengthEf(FloatLength(12.5f, LengthType::Percent), 32.0f) == 4.0f);
    Require(_ZN7WebCore19floatValueForLengthERKNS_6LengthEf(IntLength(0, LengthType::Auto), 77.5f) == 77.5f);
    Require(_ZN7WebCore19floatValueForLengthERKNS_6LengthEf(IntLength(0, LengthType::FillAvailable), 77.5f) == 77.5f);
    for (auto type : {LengthType::Relative, LengthType::Intrinsic, LengthType::MinIntrinsic, LengthType::MinContent, LengthType::MaxContent,
                      LengthType::FitContent, LengthType::Content, LengthType::Undefined})
        Require(_ZN7WebCore19floatValueForLengthERKNS_6LengthEf(IntLength(7, type), 100.0f) == 0.0f);
    Require(Throws([&] { _ZN7WebCore19floatValueForLengthERKNS_6LengthEf(IntLength(1, LengthType::Calculated), 100.0f); }));

    Require(ValueForLength(IntLength(10, LengthType::Fixed), 12800) == 640);
    Require(ValueForLength(FloatLength(1.5f, LengthType::Fixed), 0) == 96);
    Require(ValueForLength(FloatLength(-1.5f, LengthType::Fixed), 0) == -96);
    Require(ValueForLength(FloatLength(0.01f, LengthType::Fixed), 0) == 0);
    Require(ValueForLength(FloatLength(1e12f, LengthType::Fixed), 0) == std::numeric_limits<int>::max());
    Require(ValueForLength(FloatLength(-1e12f, LengthType::Fixed), 0) == std::numeric_limits<int>::min());
    Require(ValueForLength(IntLength(50, LengthType::Percent), 12800) == 6400);
    Require(ValueForLength(IntLength(25, LengthType::Percent), 672) == 168);
    Require(ValueForLength(IntLength(0, LengthType::Auto), 4321) == 4321);
    Require(ValueForLength(IntLength(0, LengthType::FillAvailable), -77) == -77);
    for (auto type : {LengthType::Relative, LengthType::Intrinsic, LengthType::MinIntrinsic, LengthType::MinContent, LengthType::MaxContent,
                      LengthType::FitContent, LengthType::Content, LengthType::Undefined})
        Require(ValueForLength(IntLength(7, type), 640) == 0);
    Require(Throws([&] { ValueForLength(IntLength(1, LengthType::Calculated), 640); }));
}

static WebCore::FloatQuad Quad(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4) {
    return WebCore::FloatQuad{{x1, y1}, {x2, y2}, {x3, y3}, {x4, y4}};
}

static WebCore::FloatRect United(const WebCore::FloatQuad* quads, unsigned size) {
    WebCore::FloatQuadVector vector{quads, size, size};
    return _ZN7WebCore19unitedBoundingBoxesERKN3WTF6VectorINS_9FloatQuadELm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE(vector);
}

static bool RectIs(const WebCore::FloatRect& rect, float x, float y, float width, float height) {
    return rect.m_location.m_x == x && rect.m_location.m_y == y && rect.m_size.m_width == width && rect.m_size.m_height == height;
}

static void TestUnitedBoundingBoxes() {
    Require(RectIs(United(nullptr, 0), 0, 0, 0, 0));
    WebCore::FloatQuad quads[] = {
        Quad(1, 2, 5, 3, 4, 9, 2, 7),
        Quad(10, -3, 12, -3, 12, 0, 10, 0),
        Quad(20, 20, 20, 20, 20, 20, 20, 20),
    };
    Require(RectIs(United(quads, 1), 1, 2, 4, 7));
    Require(RectIs(United(quads, 2), 1, -3, 11, 12));
    Require(RectIs(United(quads, 3), 1, -3, 19, 23));
    Require(RectIs(United(quads + 2, 1), 20, 20, 0, 0));
    Require(Throws([&] { United(nullptr, 2); }));
}

int main() {
    TestGridPosition();
    TestLengthFunctions();
    TestUnitedBoundingBoxes();
    return 0;
}
