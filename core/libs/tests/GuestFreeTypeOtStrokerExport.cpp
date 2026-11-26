#include "prx/libSceFreeTypeOt/libSceFreeTypeOt.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"

#include FT_STROKER_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace FreeType;

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FreeTypeOt stroker export: %s\n", message);
    std::exit(1);
}

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
    FT_Stroker native = nullptr;
    Require(FT_Stroker_New(library.host, &native) == 0, "create reference stroker");
    FT_Stroker_Set(native, 64, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
    FT_Vector start{0, 0};
    FT_Vector end{640, 0};
    Require(FT_Stroker_BeginSubPath(native, &start, 1) == 0 &&
            FT_Stroker_LineTo(native, &end) == 0 && FT_Stroker_EndSubPath(native) == 0, "reference stroke");
    std::uint32_t points = 0;
    std::uint32_t contours = 0;
    Require(FT_Stroker_GetCounts_nid_postfix(forwarded, &points, &contours) == 0 && points && contours, "count stroke");
    FT_UInt nativePoints = 0;
    FT_UInt nativeContours = 0;
    Require(FT_Stroker_GetCounts(native, &nativePoints, &nativeContours) == 0 &&
            nativePoints == points && nativeContours == contours, "count reference stroke");
    Target actual(points, contours);
    std::vector<FT_Vector> expectedPoints(points);
    std::vector<unsigned char> expectedTags(points);
    std::vector<std::uint16_t> expectedContours(contours);
    FT_Outline expected{0, 0, expectedPoints.data(), expectedTags.data(), expectedContours.data(), 0};
    FT_Stroker_ExportForward_nid_no_patch(forwarded, &actual.outline);
    FT_Stroker_Export(native, &expected);
    Require(actual.outline.n_points == expected.n_points && actual.outline.n_contours == expected.n_contours, "export counts");
    for (std::uint32_t i = 0; i < points; ++i)
        Require(actual.points[i].x == expectedPoints[i].x && actual.points[i].y == expectedPoints[i].y &&
                actual.tags[i] == expectedTags[i], "export points and tags");
    Require(actual.contours == expectedContours, "export contour ends");
    FT_Stroker_Done_nid_postfix(forwarded);
    FT_Stroker_Done(native);
    Require(FT_Done_FreeType(library.host) == 0, "release FreeType");
    std::puts("FreeTypeOt stroker export tests passed");
    return 0;
}
