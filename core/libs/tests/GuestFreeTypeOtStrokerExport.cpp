#include "prx/libSceFreeTypeOt/libSceFreeTypeOt.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"

#include FT_STROKER_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

using namespace FreeType;

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FreeTypeOt stroker export: %s\n", message);
    std::exit(1);
}

constexpr bool SharedPoints = sizeof(FT_Pos) == sizeof(std::int64_t);

struct Target {
    std::vector<GuestVector> points;
    std::vector<unsigned char> tags;
    std::vector<std::uint16_t> contours;
    GuestOutline outline{};

    Target(std::uint32_t pointCount, std::uint32_t contourCount)
        : points(pointCount), tags(pointCount), contours(contourCount) {
        outline = {0, 0, points.data(), tags.data(), contours.data(), 0};
    }
};

void* Stroke(GuestLibrary& library) {
    void* stroker = nullptr;
    Require(FT_Stroker_New_nid_postfix(&library, &stroker) == 0 && stroker, "create stroker");
    FT_Stroker_Set_nid_postfix(stroker, 64, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
    const GuestVector start{0, 0};
    const GuestVector end{640, 0};
    Require(FT_Stroker_BeginSubPath_nid_postfix(stroker, &start, 1) == 0, "begin subpath");
    Require(FT_Stroker_LineTo_nid_postfix(stroker, &end) == 0, "line to");
    Require(FT_Stroker_EndSubPath_nid_postfix(stroker) == 0, "end subpath");
    return stroker;
}

}

int main() {
    GuestLibrary library{};
    Require(FT_Init_FreeType(&library.host) == 0, "initialize FreeType");
    void* forwarded = Stroke(library);
    void* direct = Stroke(library);
    std::uint32_t points = 0;
    std::uint32_t contours = 0;
    Require(FT_Stroker_GetCounts_nid_postfix(forwarded, &points, &contours) == 0 && points && contours, "count stroke");
    std::uint32_t directPoints = 0;
    std::uint32_t directContours = 0;
    Require(FT_Stroker_GetCounts_nid_postfix(direct, &directPoints, &directContours) == 0 &&
            directPoints == points && directContours == contours, "count reference stroke");
    Target actual(points, contours);
    Target expected(points, contours);
    if constexpr (SharedPoints) {
        FT_Stroker_ExportForward_nid_no_patch(forwarded, &actual.outline);
        FT_Stroker_Export_nid_postfix(direct, &expected.outline);
        Require(actual.outline.n_points == points && actual.outline.n_contours == contours, "export counts");
        Require(actual.outline.n_points == expected.outline.n_points &&
                actual.outline.n_contours == expected.outline.n_contours, "counts match libSceFreeType");
        for (std::uint32_t i = 0; i < points; ++i)
            Require(actual.points[i].x == expected.points[i].x && actual.points[i].y == expected.points[i].y &&
                    actual.tags[i] == expected.tags[i], "points match libSceFreeType");
        for (std::uint32_t i = 0; i < contours; ++i)
            Require(actual.contours[i] == expected.contours[i], "contours match libSceFreeType");
    } else {
        bool threw = false;
        try {
            FT_Stroker_ExportForward_nid_no_patch(forwarded, &actual.outline);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw && actual.outline.n_points == 0 && actual.outline.n_contours == 0, "32-bit FT_Pos export throws");
    }
    FT_Stroker_Done_nid_postfix(forwarded);
    FT_Stroker_Done_nid_postfix(direct);
    Require(FT_Done_FreeType(library.host) == 0, "release FreeType");
    std::puts("FreeTypeOt stroker export tests passed");
    return 0;
}
