#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebCoreLayoutExports.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

namespace {

using NKWebKit::WtfAbi::String;

constexpr int GridMaxPosition = 1000000;
std::optional<int> g_maxPositionForTesting;

template<typename T> T& Checked(T* value) {
    if (!value) throw std::invalid_argument("null WebCore object");
    return *value;
}

int GridMax() {
    return g_maxPositionForTesting.value_or(GridMaxPosition);
}

int ClampGridPosition(int value) {
    int max = GridMax();
    int min = -max;
    if (value >= max) return max;
    if (value <= min) return min;
    return value;
}

void Assign(String& target, const String& source) {
    String retained(source);
    std::swap(target.impl, retained.impl);
}

int ClampToInteger(double value) {
    if (value >= static_cast<double>(std::numeric_limits<int>::max())) return std::numeric_limits<int>::max();
    if (!(value > static_cast<double>(std::numeric_limits<int>::min()))) return std::numeric_limits<int>::min();
    return static_cast<int>(value);
}

constexpr int FixedPointDenominator = 64;

float LengthValue(const WebCore::Length& length) {
    return length.m_isFloat ? length.m_floatValue : static_cast<float>(length.m_intValue);
}

[[noreturn]] void ThrowCalculatedLength() {
    throw std::invalid_argument("calculated WebCore length");
}

float Min4(float a, float b, float c, float d) {
    return std::min(std::min(a, b), std::min(c, d));
}

float Max4(float a, float b, float c, float d) {
    return std::max(std::max(a, b), std::max(c, d));
}

WebCore::FloatRect BoundingBox(const WebCore::FloatQuad& quad) {
    float left = Min4(quad.m_p1.m_x, quad.m_p2.m_x, quad.m_p3.m_x, quad.m_p4.m_x);
    float top = Min4(quad.m_p1.m_y, quad.m_p2.m_y, quad.m_p3.m_y, quad.m_p4.m_y);
    float right = Max4(quad.m_p1.m_x, quad.m_p2.m_x, quad.m_p3.m_x, quad.m_p4.m_x);
    float bottom = Max4(quad.m_p1.m_y, quad.m_p2.m_y, quad.m_p3.m_y, quad.m_p4.m_y);
    return WebCore::FloatRect{WebCore::FloatPoint{left, top}, WebCore::FloatSize{right - left, bottom - top}};
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore12GridPosition19setExplicitPositionEiRKN3WTF6StringE(WebCore::GridPosition* self, int position, const String& namedGridLine) {
    auto& grid = Checked(self);
    grid.m_type = WebCore::ExplicitPosition;
    grid.m_integerPosition = ClampGridPosition(position);
    Assign(grid.m_namedGridLine, namedGridLine);
}

void APS5_VABI _ZN7WebCore12GridPosition15setSpanPositionEiRKN3WTF6StringE(WebCore::GridPosition* self, int position, const String& namedGridLine) {
    auto& grid = Checked(self);
    grid.m_type = WebCore::SpanPosition;
    grid.m_integerPosition = ClampGridPosition(position);
    Assign(grid.m_namedGridLine, namedGridLine);
}

void APS5_VABI _ZN7WebCore12GridPosition24setMaxPositionForTestingEj(unsigned maxPosition) {
    g_maxPositionForTesting = static_cast<int>(maxPosition);
}

float APS5_VABI _ZN7WebCore19floatValueForLengthERKNS_6LengthEf(const WebCore::Length& length, float maximumValue) {
    switch (length.m_type) {
    case WebCore::LengthType::Fixed:
        return LengthValue(length);
    case WebCore::LengthType::Percent:
        return static_cast<float>(maximumValue * LengthValue(length) / 100.0f);
    case WebCore::LengthType::FillAvailable:
    case WebCore::LengthType::Auto:
        return maximumValue;
    case WebCore::LengthType::Calculated:
        ThrowCalculatedLength();
    default:
        return 0;
    }
}

WebCore::LayoutUnit APS5_VABI _ZN7WebCore14valueForLengthERKNS_6LengthENS_10LayoutUnitE(const WebCore::Length& length, WebCore::LayoutUnit maximumValue) {
    switch (length.m_type) {
    case WebCore::LengthType::Fixed:
        return WebCore::LayoutUnit{ClampToInteger(static_cast<double>(LengthValue(length)) * FixedPointDenominator)};
    case WebCore::LengthType::Percent: {
        float maximum = static_cast<float>(maximumValue.m_value) / FixedPointDenominator;
        float value = static_cast<float>(maximum * LengthValue(length) / 100.0f);
        return WebCore::LayoutUnit{ClampToInteger(static_cast<double>(value) * FixedPointDenominator)};
    }
    case WebCore::LengthType::FillAvailable:
    case WebCore::LengthType::Auto:
        return maximumValue;
    case WebCore::LengthType::Calculated:
        ThrowCalculatedLength();
    default:
        return WebCore::LayoutUnit{0};
    }
}

WebCore::FloatRect APS5_VABI _ZN7WebCore19unitedBoundingBoxesERKN3WTF6VectorINS_9FloatQuadELm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE(const WebCore::FloatQuadVector& quads) {
    if (!quads.m_size) return WebCore::FloatRect{};
    if (!quads.m_buffer) throw std::invalid_argument("null WTF vector buffer");
    WebCore::FloatRect result = BoundingBox(quads.m_buffer[0]);
    for (unsigned i = 1; i < quads.m_size; ++i) {
        WebCore::FloatRect other = BoundingBox(quads.m_buffer[i]);
        float minX = std::min(result.m_location.m_x, other.m_location.m_x);
        float minY = std::min(result.m_location.m_y, other.m_location.m_y);
        float maxX = std::max(result.m_location.m_x + result.m_size.m_width, other.m_location.m_x + other.m_size.m_width);
        float maxY = std::max(result.m_location.m_y + result.m_size.m_height, other.m_location.m_y + other.m_size.m_height);
        result.m_location = WebCore::FloatPoint{minX, minY};
        result.m_size = WebCore::FloatSize{maxX - minX, maxY - minY};
    }
    return result;
}

}
