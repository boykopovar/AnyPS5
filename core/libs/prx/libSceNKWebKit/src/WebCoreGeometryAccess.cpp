#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {

int ClampFloatToInteger(float value) {
    if (value >= static_cast<float>(std::numeric_limits<int>::max())) {
        return std::numeric_limits<int>::max();
    }
    if (!(value > static_cast<float>(std::numeric_limits<int>::min()))) {
        return std::numeric_limits<int>::min();
    }
    return static_cast<int>(value);
}

int ClampRaw(std::int64_t raw) {
    return static_cast<int>(std::clamp<std::int64_t>(raw,
        std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));
}

int FloorLayout(int raw) {
    return raw / 64 - (raw < 0 && raw % 64 != 0);
}

int CeilLayoutSum(int first, int second) {
    const auto sum = std::clamp<std::int64_t>(static_cast<std::int64_t>(first) + second,
        std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
    const auto rounded = sum / 64 + (sum > 0 && sum % 64 != 0);
    return static_cast<int>(std::min(rounded, static_cast<std::int64_t>(std::numeric_limits<int>::max() / 64)));
}

} // namespace

extern "C" {

WebCore::LayoutRect APS5_VABI _ZN7WebCore19enclosingLayoutRectERKNS_9FloatRectE(const WebCore::FloatRect& rect) {
    const int left = ClampFloatToInteger(std::floor(rect.m_location.m_x * 64.0f));
    const int top = ClampFloatToInteger(std::floor(rect.m_location.m_y * 64.0f));
    const int right = ClampFloatToInteger(std::ceil((rect.m_location.m_x + rect.m_size.m_width) * 64.0f));
    const int bottom = ClampFloatToInteger(std::ceil((rect.m_location.m_y + rect.m_size.m_height) * 64.0f));
    return {{{left}, {top}}, {{ClampRaw(static_cast<std::int64_t>(right) - left)},
        {ClampRaw(static_cast<std::int64_t>(bottom) - top)}}};
}

WebCore::AffineTransform APS5_VABI _ZN7WebCore19makeMapBetweenRectsERKNS_9FloatRectES2_(
    const WebCore::FloatRect& source, const WebCore::FloatRect& destination) {
    WebCore::AffineTransform result;
    result.m_transform[4] += destination.m_location.m_x - source.m_location.m_x;
    result.m_transform[5] += destination.m_location.m_y - source.m_location.m_y;
    const double scaleX = destination.m_size.m_width / source.m_size.m_width;
    const double scaleY = destination.m_size.m_height / source.m_size.m_height;
    result.m_transform[0] *= scaleX;
    result.m_transform[1] *= scaleX;
    result.m_transform[2] *= scaleY;
    result.m_transform[3] *= scaleY;
    return result;
}

WebCore::FloatRect APS5_VABI _ZN7WebCore25encloseRectToDevicePixelsERKNS_9FloatRectEf(
    const WebCore::FloatRect& rect, float scale) {
    const float left = std::floor(rect.m_location.m_x * scale) / scale;
    const float top = std::floor(rect.m_location.m_y * scale) / scale;
    const float right = std::ceil((rect.m_location.m_x + rect.m_size.m_width) * scale) / scale;
    const float bottom = std::ceil((rect.m_location.m_y + rect.m_size.m_height) * scale) / scale;
    return {{left, top}, {right - left, bottom - top}};
}

WebCore::IntRect APS5_VABI _ZN7WebCore16enclosingIntRectERKNS_10LayoutRectE(const WebCore::LayoutRect& rect) {
    const int left = FloorLayout(rect.m_location.m_x.m_value);
    const int top = FloorLayout(rect.m_location.m_y.m_value);
    const int right = rect.m_size.m_width.m_value == 0 ? left
        : CeilLayoutSum(rect.m_location.m_x.m_value, rect.m_size.m_width.m_value);
    const int bottom = rect.m_size.m_height.m_value == 0 ? top
        : CeilLayoutSum(rect.m_location.m_y.m_value, rect.m_size.m_height.m_value);
    return {{left, top}, {right - left, bottom - top}};
}

WebCore::IntRect APS5_VABI _ZN7WebCore16enclosingIntRectERKNS_9FloatRectE(const WebCore::FloatRect& rect) {
    const float left = static_cast<float>(ClampFloatToInteger(std::floor(rect.m_location.m_x)));
    const float top = static_cast<float>(ClampFloatToInteger(std::floor(rect.m_location.m_y)));
    const float right = static_cast<float>(ClampFloatToInteger(std::ceil(rect.m_location.m_x + rect.m_size.m_width)));
    const float bottom = static_cast<float>(ClampFloatToInteger(std::ceil(rect.m_location.m_y + rect.m_size.m_height)));
    return {{ClampFloatToInteger(left), ClampFloatToInteger(top)},
        {ClampFloatToInteger(right - left), ClampFloatToInteger(bottom - top)}};
}

bool APS5_VABI _ZN7WebCore16findIntersectionERKNS_10FloatPointES2_S2_S2_RS0_(
    const WebCore::FloatPoint& firstStart, const WebCore::FloatPoint& firstEnd,
    const WebCore::FloatPoint& secondStart, const WebCore::FloatPoint& secondEnd,
    WebCore::FloatPoint& intersection) {
    const float firstX = firstEnd.m_x - firstStart.m_x;
    const float firstY = firstEnd.m_y - firstStart.m_y;
    const float secondX = secondEnd.m_x - secondStart.m_x;
    const float secondY = secondEnd.m_y - secondStart.m_y;
    const float determinant = firstX * secondY - firstY * secondX;
    if (determinant == 0.0f) {
        return false;
    }
    const float position = ((secondStart.m_x - firstStart.m_x) * secondY
        - (secondStart.m_y - firstStart.m_y) * secondX) / determinant;
    intersection.m_x = firstStart.m_x + position * firstX;
    intersection.m_y = firstStart.m_y + position * firstY;
    return true;
}

}
