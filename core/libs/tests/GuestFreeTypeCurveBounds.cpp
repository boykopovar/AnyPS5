#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestFreeType.hpp"

#include <ft2build.h>
#include FT_BBOX_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <random>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace FreeType;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "FreeType curve bounds check failed at line %u\n", location.line());
        std::abort();
    }
}

bool Equal(const GuestBBox& a, const GuestBBox& b) {
    return a.xMin == b.xMin && a.yMin == b.yMin && a.xMax == b.xMax && a.yMax == b.yMax;
}

struct GuardedBox {
    std::uint64_t before = 0x123456789ABCDEF0;
    GuestBBox box{101, -102, 103, -104};
    std::uint64_t after = 0xFEDCBA9876543210;
};

GuestBBox Check(std::vector<GuestVector> points, std::vector<unsigned char> tags,
    std::vector<std::uint16_t> contours, std::int32_t flags = 0, bool failure = false) {
    Require(points.size() == tags.size() && points.size() <= 65535 && contours.size() <= 65535);
    const auto originalPoints = points;
    const auto originalTags = tags;
    const auto originalContours = contours;
    std::vector<FT_Vector> nativePoints;
    for (const auto& point : points) nativePoints.push_back({static_cast<FT_Pos>(point.x), static_cast<FT_Pos>(point.y)});
    auto nativeTags = tags;
    auto nativeContours = contours;
    FT_Outline native{static_cast<FT_UShort>(contours.size()), static_cast<FT_UShort>(points.size()),
        nativePoints.data(), nativeTags.data(), nativeContours.data(), flags};
    GuestOutline guest{native.n_contours, native.n_points, points.data(), tags.data(), contours.data(), flags};
    const auto original = guest;
    FT_BBox expected{101, -102, 103, -104};
    GuardedBox actual;
    const auto status = FT_Outline_Get_BBox(&native, &expected);
    Require(FT_Outline_Get_BBox_nid_postfix(&guest, &actual.box) == status);
    Require((status != 0) == failure);
    Require(Equal(actual.box, {expected.xMin, expected.yMin, expected.xMax, expected.yMax}));
    Require(actual.before == 0x123456789ABCDEF0 && actual.after == 0xFEDCBA9876543210);
    Require(guest.n_points == original.n_points && guest.n_contours == original.n_contours && guest.flags == flags);
    Require(guest.points == original.points && guest.tags == original.tags && guest.contours == original.contours);
    Require(tags == originalTags && contours == originalContours);
    for (std::size_t index = 0; index < points.size(); ++index)
        Require(points[index].x == originalPoints[index].x && points[index].y == originalPoints[index].y);
    return actual.box;
}

int Measure(std::vector<GuestVector> points, std::vector<unsigned char> tags, std::vector<std::uint16_t> contours, GuestBBox& box) {
    GuestOutline guest{static_cast<std::uint16_t>(contours.size()), static_cast<std::uint16_t>(points.size()),
        points.data(), tags.data(), contours.data(), 0};
    GuardedBox actual;
    const int status = FT_Outline_Get_BBox_nid_postfix(&guest, &actual.box);
    Require(actual.before == 0x123456789ABCDEF0 && actual.after == 0xFEDCBA9876543210);
    box = actual.box;
    return status;
}

GuestBBox Expect(std::vector<GuestVector> points, std::vector<unsigned char> tags, std::vector<std::uint16_t> contours) {
    GuestBBox box{};
    Require(Measure(std::move(points), std::move(tags), std::move(contours), box) == 0);
    return box;
}

bool Rejected(GuestOutline outline, const std::string& reason) {
    GuardedBox actual;
    const auto sentinel = actual.box;
    try {
        FT_Outline_Get_BBox_nid_postfix(&outline, &actual.box);
    } catch (const std::invalid_argument& error) {
        return std::string(error.what()).starts_with(reason) && Equal(actual.box, sentinel);
    }
    return false;
}

int main() {
    GuardedBox invalid;
    const auto sentinel = invalid.box;
    FT_BBox native{};
    Require(FT_Outline_Get_BBox_nid_postfix(nullptr, &invalid.box) == FT_Outline_Get_BBox(nullptr, &native));
    Require(Equal(invalid.box, sentinel));
    Require(FT_Outline_Get_BBox_nid_postfix(reinterpret_cast<GuestOutline*>(std::uintptr_t{1}), nullptr) == FT_Outline_Get_BBox(nullptr, nullptr));
    Check({}, {}, {});
    GuestOutline empty{0, 3, reinterpret_cast<GuestVector*>(std::uintptr_t{1}),
        reinterpret_cast<unsigned char*>(std::uintptr_t{1}), reinterpret_cast<std::uint16_t*>(std::uintptr_t{1}), 123};
    Require(FT_Outline_Get_BBox_nid_postfix(&empty, &invalid.box) == 0 && Equal(invalid.box, {}));
    empty.n_contours = 1;
    empty.n_points = 0;
    Require(FT_Outline_Get_BBox_nid_postfix(&empty, &invalid.box) == 0 && Equal(invalid.box, {}));
    Require(Equal(Check({{-20, 30}, {40, -50}, {70, 80}}, {1, 1, 1}, {2}), {-20, -50, 70, 80}));
    Require(Equal(Check({{0, 0}, {100, 200}, {200, 0}}, {1, 0, 1}, {2}), {0, 0, 200, 100}));
    Require(Equal(Check({{0, 0}, {0, 400}, {400, 400}, {400, 0}}, {1, 2, 2, 1}, {3}), {0, 0, 400, 300}));
    Check({{0, 0}, {100, 200}, {200, 200}, {300, 0}}, {1, 0, 0, 1}, {3}, -1);
    Check({{-100, 0}, {0, 200}, {100, 0}, {0, -200}}, {0, 0, 0, 0}, {3});
    Check({{0, 0}, {100, 200}, {200, 0}, {-30, -40}, {-20, -50}, {10, -10}}, {1, 0, 1, 1, 1, 1}, {2, 5});
    Check({{0, 100}, {100, 0}, {200, 0}}, {2, 1, 1}, {2}, 0, true);
    Check({{0, 0}, {100, 200}}, {1, 2}, {1}, 0, true);
    Check({{std::numeric_limits<std::int32_t>::min(), 1}, {std::numeric_limits<std::int32_t>::max(), -1}}, {1, 1}, {1});
    constexpr std::int64_t curveLimit = std::numeric_limits<std::int32_t>::max() / 4;
    Require(Equal(Check({{-curveLimit, -curveLimit}, {0, curveLimit}, {curveLimit, -curveLimit}}, {1, 0, 1}, {2}),
        {-curveLimit, -curveLimit, curveLimit, 0}));

    std::mt19937 random(0xB0A1D5);
    for (unsigned iteration = 0; iteration < 4096; ++iteration) {
        std::vector<GuestVector> points;
        for (unsigned index = 0; index < 4; ++index)
            points.push_back({static_cast<std::int64_t>(random() % 100001) - 50000, static_cast<std::int64_t>(random() % 100001) - 50000});
        Check(points, {1, 2, 2, 1}, {3});
        points.pop_back();
        Check(points, {1, 0, 1}, {2});
    }
    std::vector<GuestVector> many(65535);
    std::vector<unsigned char> tags(many.size(), 1);
    std::vector<std::uint16_t> contours(many.size());
    for (std::size_t index = 0; index < many.size(); ++index) {
        many[index] = {static_cast<std::int64_t>(index % 1000), static_cast<std::int64_t>(index)};
        contours[index] = static_cast<std::uint16_t>(index);
    }
    Require(Equal(Check(many, tags, {65534}), {0, 0, 999, 65534}));
    Require(Equal(Check(many, tags, contours), {0, 0, 999, 65534}));

    std::array<GuestVector, 3> points{{{0, 0}, {100, 200}, {200, 0}}};
    std::array<unsigned char, 3> curvedTags{1, 0, 1};
    std::uint16_t end = 2;
    for (int missing = 0; missing < 3; ++missing) {
        GuestOutline outline{1, 3, missing == 0 ? nullptr : points.data(), missing == 1 ? nullptr : curvedTags.data(),
            missing == 2 ? nullptr : &end, 0};
        Require(Rejected(outline, "FT_Outline_Get_BBox: null "));
    }
    for (std::uint16_t endpoint : {std::uint16_t{3}, std::uint16_t{65535}}) {
        GuestOutline outline{1, 3, points.data(), curvedTags.data(), &endpoint, 0};
        Require(Rejected(outline, "FT_Outline_Decompose: contour endpoint at or beyond n_points"));
    }
    std::array<unsigned char, 3> onTags{1, 1, 1};
    GuestOutline straight{1, 3, points.data(), onTags.data(), nullptr, 0};
    Require(FT_Outline_Get_BBox_nid_postfix(&straight, &invalid.box) == 0 && Equal(invalid.box, {0, 0, 200, 200}));
    for (auto contours : std::vector<std::vector<std::uint16_t>>{{3}, {0}, {1, 1}, {1, 0}, {0, 1, 1}, {65535}})
        Require(Equal(Check({{0, 0}, {100, 200}, {200, 0}}, {1, 1, 1}, contours), {0, 0, 200, 200}));
    Require(Equal(Check({{0, 0}, {100, 200}, {200, 0}, {500, -500}}, {1, 0, 1, 1}, {2}), {0, -500, 500, 100}));
    Check({{0, 0}, {100, 200}, {200, 0}, {500, -500}}, {1, 0, 1, 1}, {2, 1}, 0, true);
    Check({{0, 0}, {100, 200}, {200, 0}, {500, -500}}, {1, 0, 1, 1}, {2, 2}, 0, true);
    GuestBBox unchanged{};
    Require(Measure({{0, 100}, {100, 0}, {200, 0}}, {2, 2, 1}, {2}, unchanged) == FT_Err_Invalid_Outline);
    Require(Measure({{0, 100}, {100, 0}, {200, 0}, {300, 50}}, {2, 2, 1, 2}, {3}, unchanged) == FT_Err_Invalid_Outline);

    constexpr std::int64_t big = (std::int64_t{1} << 40) + 4097;
    Require(Equal(Expect({{0, 0}, {big, 2 * big}, {2 * big, 0}}, {1, 0, 1}, {2}), {0, 0, 2 * big, std::int64_t{1} << 41}));
    Require(Equal(Expect({{0, 0}, {big, -3 * big}, {3 * big, -big}, {4 * big, 0}}, {1, 2, 2, 1}, {3}),
        {0, -1742130928311, 4 * big, 0}));
    Require(Equal(Expect({{-big, 7}, {big, 9}}, {1, 1}, {1}), {-big, 7, big, 9}));
    constexpr auto low = std::numeric_limits<std::int64_t>::min();
    constexpr auto high = std::numeric_limits<std::int64_t>::max();
    for (const auto& overflow : std::vector<std::vector<GuestVector>>{{{0, low}, {0, high}, {0, low}},
             {{0, high}, {0, low}, {0, high}}, {{0, 0}, {0, high}, {0, low}}}) {
        std::array<GuestVector, 3> values{overflow[0], overflow[1], overflow[2]};
        GuestOutline outline{1, 3, values.data(), curvedTags.data(), &end, 0};
        Require(Rejected(outline, "FT_Outline_Get_BBox: signed overflow in the extremum arithmetic"));
    }
    std::array<GuestVector, 4> aligned{{{0, 0}, {0, std::int64_t{1} << 33}, {0, std::int64_t{1} << 33}, {0, 0}}};
    std::array<unsigned char, 4> cubicTags{1, 2, 2, 1};
    std::uint16_t cubicEnd = 3;
    GuestOutline zeroLow{1, 4, aligned.data(), cubicTags.data(), &cubicEnd, 0};
    Require(Rejected(zeroLow, "FT_Outline_Get_BBox: cubic control distances whose low 32 bits are zero"));

    if constexpr (sizeof(FT_Pos) >= sizeof(std::int64_t)) {
        Check({{std::int64_t{1} << 40, -(std::int64_t{1} << 40)}, {0, 0}}, {1, 1}, {1});
        Check({{0, 0}, {big, 2 * big}, {2 * big, 0}}, {1, 0, 1}, {2});
    }
    std::mt19937_64 wideRandom(0x5EED);
    std::uint64_t digest = 0xCBF29CE484222325;
    const auto fold = [&digest](std::int64_t value) { digest = (digest ^ static_cast<std::uint64_t>(value)) * 0x100000001B3; };
    const auto record = [&fold](std::vector<GuestVector> curve, std::vector<unsigned char> curveTags, std::uint16_t endpoint) {
        GuestBBox box{};
        fold(Measure(std::move(curve), std::move(curveTags), {endpoint}, box));
        fold(box.xMin);
        fold(box.yMin);
        fold(box.xMax);
        fold(box.yMax);
    };
    for (unsigned iteration = 0; iteration < 4096; ++iteration) {
        std::vector<GuestVector> wide;
        for (unsigned index = 0; index < 4; ++index) {
            const auto magnitude = std::uint64_t{1} << (20 + wideRandom() % 31);
            wide.push_back({static_cast<std::int64_t>(wideRandom() % magnitude) - static_cast<std::int64_t>(magnitude / 2),
                static_cast<std::int64_t>(wideRandom() % magnitude) - static_cast<std::int64_t>(magnitude / 2)});
        }
        record(wide, {1, 2, 2, 1}, 3);
        wide.pop_back();
        record(wide, {1, 0, 1}, 2);
    }
    Require(digest == 0x6C9B1F2FED2DF477);
    std::printf("FreeType curve bounds: 16384 generated curves, extrema, errors and count limits passed\n");
}
