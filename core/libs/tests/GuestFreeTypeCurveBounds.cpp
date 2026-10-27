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

    std::array<GuestVector, 2> points{{{0, 0}, {1, 1}}};
    std::array<unsigned char, 2> smallTags{1, 1};
    std::uint16_t end = 1;
    GuestOutline missing{1, 2, nullptr, smallTags.data(), &end, 0};
    invalid.box = sentinel;
    bool threw = false;
    try {
        FT_Outline_Get_BBox_nid_postfix(&missing, &invalid.box);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw && Equal(invalid.box, sentinel));
    for (auto badContours : std::vector<std::vector<std::uint16_t>>{{2}, {0}, {1, 1}, {1, 0}, {0, 1, 1}}) {
        GuestOutline malformed{static_cast<std::uint16_t>(badContours.size()), 2,
            points.data(), smallTags.data(), badContours.data(), 0};
        threw = false;
        try {
            FT_Outline_Get_BBox_nid_postfix(&malformed, &invalid.box);
        } catch (const std::runtime_error& error) {
            threw = std::string(error.what()).starts_with("FT_Outline_Get_BBox: inconsistent contour bounds");
        }
        Require(threw && Equal(invalid.box, sentinel));
    }
    if constexpr (sizeof(FT_Pos) >= sizeof(std::int64_t)) {
        Check({{std::int64_t{1} << 40, -(std::int64_t{1} << 40)}, {0, 0}}, {1, 1}, {1});
    }
    std::printf("FreeType curve bounds: 8192 generated curves, extrema, errors and count limits passed\n");
}
