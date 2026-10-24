#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <ft2build.h>
#include FT_OUTLINE_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

using namespace FreeType;

extern "C" {
int APS5_VABI FT_Outline_Check_nid_postfix(GuestOutline*);
void APS5_VABI FT_Outline_Get_CBox_nid_postfix(const GuestOutline*, GuestBBox*);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType outline check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static void CompareCheck(std::uint16_t pointCount, std::vector<std::uint16_t>& contours) {
    GuestOutline guest{static_cast<std::uint16_t>(contours.size()), pointCount, nullptr, nullptr, contours.data(), 0x71};
    FT_Outline native{guest.n_contours, pointCount, nullptr, nullptr, contours.data(), guest.flags};
    const auto before = contours;
    Require(FT_Outline_Check_nid_postfix(&guest) == FT_Outline_Check(&native));
    Require(contours == before && guest.flags == 0x71 && guest.points == nullptr && guest.tags == nullptr);
}

static void EnumerateContours(std::uint16_t pointCount, std::vector<std::uint16_t>& contours, std::size_t index) {
    if (index == contours.size()) {
        CompareCheck(pointCount, contours);
        return;
    }
    for (const std::uint16_t end : {0, 1, 2, 3, 4, 5, 6, 65535}) {
        contours[index] = end;
        EnumerateContours(pointCount, contours, index + 1);
    }
}

static bool Equal(const GuestBBox& actual, const GuestBBox& expected) {
    return actual.xMin == expected.xMin && actual.yMin == expected.yMin && actual.xMax == expected.xMax && actual.yMax == expected.yMax;
}

static void CompareBounds(std::vector<GuestVector> points) {
    std::vector<FT_Vector> nativePoints;
    for (const auto& point : points) nativePoints.push_back({static_cast<FT_Pos>(point.x), static_cast<FT_Pos>(point.y)});
    GuestOutline guest{65535, static_cast<std::uint16_t>(points.size()), points.data(), nullptr, nullptr, 0x71};
    FT_Outline native{guest.n_contours, guest.n_points, nativePoints.data(), nullptr, nullptr, guest.flags};
    struct GuardedBox {
        std::uint64_t before;
        GuestBBox value;
        std::uint64_t after;
    } actual{0x1122334455667788, {91, 92, 93, 94}, 0x8877665544332211};
    FT_BBox expected{91, 92, 93, 94};
    FT_Outline_Get_CBox(&native, &expected);
    FT_Outline_Get_CBox_nid_postfix(&guest, &actual.value);
    Require(Equal(actual.value, {expected.xMin, expected.yMin, expected.xMax, expected.yMax}));
    Require(actual.before == 0x1122334455667788 && actual.after == 0x8877665544332211);
    FT_Outline_Get_CBox_nid_postfix(&guest, nullptr);
    Require(guest.n_contours == 65535 && guest.flags == 0x71 && guest.tags == nullptr && guest.contours == nullptr);
}

int main() {
    Require(FT_Outline_Check_nid_postfix(nullptr) == FT_Outline_Check(nullptr));
    for (std::uint16_t pointCount = 0; pointCount <= 6; ++pointCount) {
        for (std::size_t contourCount = 0; contourCount <= 3; ++contourCount) {
            std::vector<std::uint16_t> contours(contourCount);
            EnumerateContours(pointCount, contours, 0);
        }
    }
    std::vector<std::uint16_t> maximum = {65534};
    CompareCheck(65535, maximum);
    GuestOutline high{1, 65535, nullptr, nullptr, maximum.data(), 0};
    Require(FT_Outline_Check_nid_postfix(&high) == 0);
    std::vector<std::uint16_t> maximumContours(65535);
    for (std::size_t index = 0; index < maximumContours.size(); ++index) maximumContours[index] = static_cast<std::uint16_t>(index);
    CompareCheck(65535, maximumContours);
    CompareBounds({});
    CompareBounds({{17, -29}});
    CompareBounds({{-10, 20}, {5000, -7000}, {30, 40}, {-200, 9000}});
    CompareBounds({{std::numeric_limits<std::int32_t>::min(), 0}, {0, std::numeric_limits<std::int32_t>::max()}});
    std::vector<GuestVector> maximumPoints(65535, {3, 7});
    maximumPoints.back() = {-12345, 54321};
    CompareBounds(maximumPoints);
    std::array<GuestVector, 3> wide = {{{std::numeric_limits<std::int64_t>::min(), 1},
                                        {std::int64_t{1} << 40, std::numeric_limits<std::int64_t>::max()},
                                        {std::numeric_limits<std::int64_t>::max(), std::numeric_limits<std::int64_t>::min()}}};
    GuestOutline outline{0, static_cast<std::uint16_t>(wide.size()), wide.data(), nullptr, nullptr, 0};
    GuestBBox box{1, 2, 3, 4};
    FT_Outline_Get_CBox_nid_postfix(&outline, &box);
    Require(Equal(box, {std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::min(),
                        std::numeric_limits<std::int64_t>::max(), std::numeric_limits<std::int64_t>::max()}));
    const GuestBBox retained = box;
    FT_Outline_Get_CBox_nid_postfix(nullptr, &box);
    Require(Equal(box, retained));
    FT_Outline_Get_CBox_nid_postfix(nullptr, nullptr);
}
