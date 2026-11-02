#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore geometry access check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

void CheckRect(const WebCore::IntRect& value, int x, int y, int width, int height) {
    Require(value.m_location.m_x == x);
    Require(value.m_location.m_y == y);
    Require(value.m_size.m_width == width);
    Require(value.m_size.m_height == height);
}

void TestLayoutRects() {
    constexpr int low = std::numeric_limits<int>::min();
    constexpr int high = std::numeric_limits<int>::max();
    struct Case {
        int x;
        int y;
        int width;
        int height;
        std::array<int, 4> expected;
    };
    constexpr std::array cases{
        Case{0, 0, 0, 0, {0, 0, 0, 0}},
        Case{80, -176, 160, 272, {1, -3, 3, 5}},
        Case{16, -16, 0, 0, {0, -1, 0, 0}},
        Case{16, -16, 0, 64, {0, -1, 0, 2}},
        Case{16, -16, 64, 0, {0, -1, 2, 0}},
        Case{160, 224, -80, -144, {2, 3, 0, -1}},
        Case{63, -1, 1, 1, {0, -1, 1, 1}},
        Case{64, -64, 64, -64, {1, -1, 1, -1}},
        Case{-65, -127, 1, 63, {-2, -2, 1, 1}},
        Case{high, low, 64, -64, {33554431, -33554432, 0, 0}},
        Case{0, 0, high, low, {0, 0, 33554431, -33554432}},
        Case{low, high, high, low, {-33554432, 33554431, 33554432, -33554431}},
        Case{high - 63, low + 63, 1, -1, {33554431, -33554432, 0, 1}},
        Case{low, high, 0, 0, {-33554432, 33554431, 0, 0}}
    };
    for (const auto& value : cases) {
        const WebCore::LayoutRect input{{{value.x}, {value.y}}, {{value.width}, {value.height}}};
        const auto result = _ZN7WebCore16enclosingIntRectERKNS_10LayoutRectE(input);
        CheckRect(result, value.expected[0], value.expected[1], value.expected[2], value.expected[3]);
        Require(input.m_location.m_x.m_value == value.x);
        Require(input.m_location.m_y.m_value == value.y);
        Require(input.m_size.m_width.m_value == value.width);
        Require(input.m_size.m_height.m_value == value.height);
    }
}

void TestFloatRects() {
    constexpr int low = std::numeric_limits<int>::min();
    constexpr int high = std::numeric_limits<int>::max();
    const float infinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float tiny = std::numeric_limits<float>::min();
    const float subnormal = std::numeric_limits<float>::denorm_min();
    struct Case {
        WebCore::FloatRect input;
        std::array<int, 4> expected;
    };
    const std::array cases{
        Case{{{0, 0}, {0, 0}}, {0, 0, 0, 0}},
        Case{{{1.25f, -2.75f}, {2.5f, 4.25f}}, {1, -3, 3, 5}},
        Case{{{0.25f, -0.25f}, {0, 0}}, {0, -1, 1, 1}},
        Case{{{2.5f, 3.5f}, {-1.25f, -2.25f}}, {2, 3, 0, -1}},
        Case{{{tiny, -tiny}, {0, 0}}, {0, -1, 1, 1}},
        Case{{{subnormal, -subnormal}, {0, 0}}, {0, -1, 1, 1}},
        Case{{{-0.0f, 0.0f}, {-0.0f, 0.0f}}, {0, 0, 0, 0}},
        Case{{{2147483520.0f, -2147483648.0f}, {128.0f, 4294967296.0f}}, {2147483520, low, 128, high}},
        Case{{{2147483648.0f, -2147483648.0f}, {0, 0}}, {high, low, 0, 0}},
        Case{{{16777216.0f, -16777216.0f}, {1.0f, -1.0f}}, {16777216, -16777216, 0, 0}},
        Case{{{nan, nan}, {1, 1}}, {low, low, 0, 0}},
        Case{{{0, 0}, {nan, nan}}, {0, 0, low, low}},
        Case{{{infinity, -infinity}, {0, 0}}, {high, low, 0, 0}},
        Case{{{0, 0}, {infinity, -infinity}}, {0, 0, high, low}},
        Case{{{infinity, -infinity}, {-infinity, infinity}}, {high, low, low, 0}},
        Case{{{-infinity, infinity}, {infinity, -infinity}}, {low, high, 0, low}}
    };
    for (const auto& value : cases) {
        const auto before = std::bit_cast<std::array<std::uint32_t, 4>>(value.input);
        const auto result = _ZN7WebCore16enclosingIntRectERKNS_9FloatRectE(value.input);
        CheckRect(result, value.expected[0], value.expected[1], value.expected[2], value.expected[3]);
        Require((std::bit_cast<std::array<std::uint32_t, 4>>(value.input) == before));
    }
}

void TestFloatToLayoutRects() {
    constexpr int low = std::numeric_limits<int>::min();
    constexpr int high = std::numeric_limits<int>::max();
    const float infinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    struct Case {
        WebCore::FloatRect input;
        std::array<int, 4> expected;
    };
    const std::array cases{
        Case{{{0, 0}, {0, 0}}, {0, 0, 0, 0}},
        Case{{{1.25f, -2.75f}, {2.5f, 4.25f}}, {80, -176, 160, 272}},
        Case{{{0.1f, -0.1f}, {0, 0}}, {6, -7, 1, 1}},
        Case{{{2.5f, 3.5f}, {-1.25f, -2.25f}}, {160, 224, -80, -144}},
        Case{{{-33554432.0f, 33554432.0f}, {67108864.0f, -67108864.0f}}, {low, high, high, low}},
        Case{{{16777216.0f, -16777216.0f}, {1, -1}}, {1073741824, -1073741824, 0, 0}},
        Case{{{nan, nan}, {1, 1}}, {low, low, 0, 0}},
        Case{{{0, 0}, {nan, nan}}, {0, 0, low, low}},
        Case{{{infinity, -infinity}, {-infinity, infinity}}, {high, low, low, 0}},
        Case{{{-0.0f, 0.0f}, {-0.0f, 0.0f}}, {0, 0, 0, 0}}
    };
    for (const auto& value : cases) {
        const auto before = std::bit_cast<std::array<std::uint32_t, 4>>(value.input);
        const auto result = _ZN7WebCore19enclosingLayoutRectERKNS_9FloatRectE(value.input);
        Require(result.m_location.m_x.m_value == value.expected[0]);
        Require(result.m_location.m_y.m_value == value.expected[1]);
        Require(result.m_size.m_width.m_value == value.expected[2]);
        Require(result.m_size.m_height.m_value == value.expected[3]);
        Require((std::bit_cast<std::array<std::uint32_t, 4>>(value.input) == before));
    }
}

void CheckFloat(float actual, float expected) {
    if (std::isnan(expected)) Require(std::isnan(actual));
    else Require(std::bit_cast<std::uint32_t>(actual) == std::bit_cast<std::uint32_t>(expected));
}

void TestDevicePixels() {
    const float infinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    struct Case {
        WebCore::FloatRect input;
        float scale;
        WebCore::FloatRect expected;
    };
    const std::array cases{
        Case{{{1.25f, -2.75f}, {2.5f, 4.25f}}, 2, {{1, -3}, {3, 4.5f}}},
        Case{{{1.25f, -2.75f}, {2.5f, 4.25f}}, 0.5f, {{0, -4}, {4, 6}}},
        Case{{{1.25f, -2.75f}, {2.5f, 4.25f}}, -2, {{1.5f, -2.5f}, {2, 4}}},
        Case{{{0.1f, 0.1f}, {0, 0}}, 2, {{0, 0}, {0.5f, 0.5f}}},
        Case{{{-0.0f, -0.0f}, {-0.0f, -0.0f}}, 2, {{-0.0f, -0.0f}, {0, 0}}},
        Case{{{1, 2}, {3, 4}}, 0, {{nan, nan}, {nan, nan}}},
        Case{{{1, 2}, {3, 4}}, -0.0f, {{nan, nan}, {nan, nan}}},
        Case{{{1, 2}, {3, 4}}, infinity, {{nan, nan}, {nan, nan}}},
        Case{{{1, 2}, {3, 4}}, nan, {{nan, nan}, {nan, nan}}},
        Case{{{infinity, -infinity}, {1, 1}}, 2, {{infinity, -infinity}, {nan, nan}}},
        Case{{{0, 0}, {std::numeric_limits<float>::max(), std::numeric_limits<float>::max()}}, 2, {{0, 0}, {infinity, infinity}}}
    };
    for (const auto& value : cases) {
        const auto before = std::bit_cast<std::array<std::uint32_t, 4>>(value.input);
        const auto result = _ZN7WebCore25encloseRectToDevicePixelsERKNS_9FloatRectEf(value.input, value.scale);
        CheckFloat(result.m_location.m_x, value.expected.m_location.m_x);
        CheckFloat(result.m_location.m_y, value.expected.m_location.m_y);
        CheckFloat(result.m_size.m_width, value.expected.m_size.m_width);
        CheckFloat(result.m_size.m_height, value.expected.m_size.m_height);
        Require((std::bit_cast<std::array<std::uint32_t, 4>>(value.input) == before));
    }
}

void TestRectangleMaps() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    struct Case {
        WebCore::FloatRect source;
        WebCore::FloatRect destination;
        std::array<double, 6> expected;
    };
    const std::array cases{
        Case{{{10, 20}, {4, 8}}, {{30, 50}, {8, 4}}, {2, 0, 0, 0.5, 20, 30}},
        Case{{{10, 20}, {-4, 8}}, {{30, 50}, {8, -4}}, {-2, -0.0, -0.0, -0.5, 20, 30}},
        Case{{{0, 0}, {3, 7}}, {{0, 0}, {1, 1}}, {0x1.555556p-2, 0, 0, 0x1.24924ap-3, 0, 0}},
        Case{{{-16777216.0f, 16777216.0f}, {1, 1}}, {{1, -1}, {1, 1}}, {1, 0, 0, 1, 16777216, -16777216}},
        Case{{{0, 0}, {0, 0}}, {{1, 2}, {1, 0}}, {infinity, nan, nan, nan, 1, 2}},
        Case{{{0, 0}, {-0.0f, 1}}, {{0, 0}, {1, -0.0f}}, {-infinity, nan, -0.0, -0.0, 0, 0}},
        Case{{{0, 0}, {1, 1}}, {{std::numeric_limits<float>::infinity(), 1}, {1, 1}}, {1, 0, 0, 1, infinity, 1}},
        Case{{{0, 0}, {1, 1}}, {{std::numeric_limits<float>::quiet_NaN(), 1}, {1, 1}}, {1, 0, 0, 1, nan, 1}}
    };
    for (const auto& value : cases) {
        const auto sourceBefore = std::bit_cast<std::array<std::uint32_t, 4>>(value.source);
        const auto destinationBefore = std::bit_cast<std::array<std::uint32_t, 4>>(value.destination);
        const auto result = _ZN7WebCore19makeMapBetweenRectsERKNS_9FloatRectES2_(value.source, value.destination);
        for (unsigned index = 0; index < 6; ++index) {
            if (std::isnan(value.expected[index])) Require(std::isnan(result.m_transform[index]));
            else Require(std::bit_cast<std::uint64_t>(result.m_transform[index]) == std::bit_cast<std::uint64_t>(value.expected[index]));
        }
        Require((std::bit_cast<std::array<std::uint32_t, 4>>(value.source) == sourceBefore));
        Require((std::bit_cast<std::array<std::uint32_t, 4>>(value.destination) == destinationBefore));
    }
    const WebCore::FloatRect same{{3, -5}, {2, 7}};
    const auto identity = _ZN7WebCore19makeMapBetweenRectsERKNS_9FloatRectES2_(same, same);
    Require((identity.m_transform == std::array<double, 6>{1, 0, 0, 1, 0, 0}));
}

void TestIntegerMapRectCallers() {
    constexpr int low = std::numeric_limits<int>::min();
    constexpr int high = std::numeric_limits<int>::max();
    struct Case {
        WebCore::IntRect input;
        double scaleX;
        double scaleY;
        double translateX;
        double translateY;
        std::array<int, 4> expected;
    };
    const std::array cases{
        Case{{{1, 2}, {3, 4}}, 1, 1, 0, 0, {1, 2, 3, 4}},
        Case{{{10, -20}, {4, 8}}, 1, 1, 0.25, -0.25, {10, -21, 5, 9}},
        Case{{{1, 2}, {3, 4}}, 2, 0.5, 0, 0, {2, 1, 6, 2}},
        Case{{{1, 2}, {3, 4}}, -2, -3, 0, 0, {-8, -18, 6, 12}},
        Case{{{low, low}, {high, high}}, 1, 1, 0, 0, {low, low, high, high}},
        Case{{{2147483520, 0}, {128, 1}}, 1, 1, 0, 0, {2147483520, 0, 128, 1}},
        Case{{{16777217, -16777217}, {1, -1}}, 1, 1, 0, 0, {16777216, -16777216, 0, 0}},
        Case{{{0, 0}, {high, high}}, 2, 2, 0, 0, {0, 0, high, high}},
        Case{{{low, low}, {high, high}}, -1, -1, 0, 0, {0, 0, high, high}}
    };
    for (const auto& value : cases) {
        const auto inputBefore = std::bit_cast<std::array<int, 4>>(value.input);
        const WebCore::AffineTransform affine{{value.scaleX, 0, 0, value.scaleY, value.translateX, value.translateY}};
        const auto affineBefore = affine.m_transform;
        const auto affineResult = _ZNK7WebCore15AffineTransform7mapRectERKNS_7IntRectE(&affine, value.input);
        CheckRect(affineResult, value.expected[0], value.expected[1], value.expected[2], value.expected[3]);
        WebCore::TransformationMatrix matrix;
        matrix.m_matrix[0][0] = value.scaleX;
        matrix.m_matrix[1][1] = value.scaleY;
        matrix.m_matrix[3][0] = value.translateX;
        matrix.m_matrix[3][1] = value.translateY;
        const auto matrixBefore = std::bit_cast<std::array<std::uint64_t, 16>>(matrix);
        const auto matrixResult = _ZNK7WebCore20TransformationMatrix7mapRectERKNS_7IntRectE(&matrix, value.input);
        CheckRect(matrixResult, value.expected[0], value.expected[1], value.expected[2], value.expected[3]);
        Require((std::bit_cast<std::array<int, 4>>(value.input) == inputBefore));
        Require(affine.m_transform == affineBefore);
        Require((std::bit_cast<std::array<std::uint64_t, 16>>(matrix) == matrixBefore));
    }
}

bool Intersect(const std::array<WebCore::FloatPoint, 4>& points, WebCore::FloatPoint& output) {
    return _ZN7WebCore16findIntersectionERKNS_10FloatPointES2_S2_S2_RS0_(points[0], points[1], points[2], points[3], output);
}

void TestIntersections() {
    struct Case {
        std::array<WebCore::FloatPoint, 4> points;
        WebCore::FloatPoint expected;
    };
    const float epsilon = 0x1p-20f;
    const float small = 0x1p-100f;
    const std::array cases{
        Case{{{{1, 1}, {3, 3}, {1, 3}, {3, 1}}}, {2, 2}},
        Case{{{{0, 0}, {1, 0}, {2, 1}, {2, 2}}}, {2, 0}},
        Case{{{{3, -4}, {3, 2}, {-2, -1}, {7, -1}}}, {3, -1}},
        Case{{{{0, 0}, {1, 1}, {0, 1}, {1, 2 + epsilon}}}, {-1048576, -1048576}},
        Case{{{{0, 0}, {1, 0}, {0, -small}, {1, small}}}, {0.5f, 0}},
        Case{{{{-1, -1}, {0, 0}, {0, 0}, {2, -2}}}, {0, 0}}
    };
    for (const auto& value : cases) {
        WebCore::FloatPoint output{97, -23};
        Require(Intersect(value.points, output));
        Require(output.m_x == value.expected.m_x);
        Require(output.m_y == value.expected.m_y);
        for (unsigned alias = 0; alias < 4; ++alias) {
            auto points = value.points;
            Require(Intersect(points, points[alias]));
            Require(points[alias].m_x == value.expected.m_x);
            Require(points[alias].m_y == value.expected.m_y);
            for (unsigned index = 0; index < 4; ++index) {
                if (index == alias) continue;
                Require(points[index].m_x == value.points[index].m_x);
                Require(points[index].m_y == value.points[index].m_y);
            }
        }
    }
    const std::array failures{
        std::array<WebCore::FloatPoint, 4>{{{0, 0}, {1, 1}, {0, 1}, {1, 2}}},
        std::array<WebCore::FloatPoint, 4>{{{0, 0}, {1, 1}, {2, 2}, {3, 3}}},
        std::array<WebCore::FloatPoint, 4>{{{1, 1}, {1, 1}, {0, 0}, {2, 3}}},
        std::array<WebCore::FloatPoint, 4>{{{0, 0}, {2, 3}, {1, 1}, {1, 1}}},
        std::array<WebCore::FloatPoint, 4>{{{-0.0f, 0.0f}, {0.0f, -0.0f}, {0, 0}, {1, 0}}}
    };
    for (const auto& points : failures) {
        auto output = std::bit_cast<WebCore::FloatPoint>(std::array<std::uint32_t, 2>{0x7fc12345, 0x80000000});
        const auto before = std::bit_cast<std::array<std::uint32_t, 2>>(output);
        Require(!Intersect(points, output));
        Require((std::bit_cast<std::array<std::uint32_t, 2>>(output) == before));
        for (unsigned alias = 0; alias < 4; ++alias) {
            auto input = points;
            const auto saved = std::bit_cast<std::array<std::uint32_t, 8>>(input);
            Require(!Intersect(input, input[alias]));
            Require((std::bit_cast<std::array<std::uint32_t, 8>>(input) == saved));
        }
    }
    WebCore::FloatPoint signedZeroOutput{1, 2};
    Require(Intersect({{{0, -0.0f}, {1, -0.0f}, {-1, 1}, {-1, -1}}}, signedZeroOutput));
    Require(signedZeroOutput.m_x == -1);
    Require(signedZeroOutput.m_y == 0);
    Require(std::signbit(signedZeroOutput.m_y));
    const float nan = std::numeric_limits<float>::quiet_NaN();
    WebCore::FloatPoint output{1, 2};
    Require(Intersect({{{0, 0}, {nan, 1}, {0, 1}, {1, 0}}}, output));
    Require(std::isnan(output.m_x));
    Require(std::isnan(output.m_y));
    const float infinity = std::numeric_limits<float>::infinity();
    Require(Intersect({{{0, 0}, {infinity, 0}, {1, 1}, {1, -1}}}, output));
    Require(std::isnan(output.m_x));
    Require(output.m_y == 0);
}

} // namespace

int main() {
    TestLayoutRects();
    TestFloatRects();
    TestIntersections();
    TestFloatToLayoutRects();
    TestDevicePixels();
    TestRectangleMaps();
    TestIntegerMapRectCallers();
    return 0;
}
