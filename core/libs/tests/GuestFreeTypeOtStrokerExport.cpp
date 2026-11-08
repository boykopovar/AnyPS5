#include "prx/libSceFreeTypeOt/libSceFreeTypeOt.hpp"

#include <ft2build.h>
#include FT_STROKER_H

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FreeTypeOt stroker export: %s\n", message);
    std::exit(1);
}

struct Output {
    std::vector<FT_Vector> points;
    std::vector<unsigned char> tags;
    std::vector<unsigned short> contours;
    FT_Outline outline{};

    explicit Output(FT_UInt pointCount, FT_UInt contourCount)
        : points(pointCount), tags(pointCount), contours(contourCount) {
        outline.points = points.data();
        outline.tags = tags.data();
        outline.contours = contours.data();
    }
};

void Prepare(FT_Stroker stroker) {
    FT_Stroker_Set(stroker, 64, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
    FT_Vector start{0, 0};
    FT_Vector end{640, 0};
    Require(FT_Stroker_BeginSubPath(stroker, &start, 1) == 0, "begin subpath");
    Require(FT_Stroker_LineTo(stroker, &end) == 0, "line to");
    Require(FT_Stroker_EndSubPath(stroker) == 0, "end subpath");
}

}

int main() {
    FT_Library library = nullptr;
    Require(FT_Init_FreeType(&library) == 0, "initialize FreeType");
    FT_Stroker expectedStroker = nullptr;
    FT_Stroker actualStroker = nullptr;
    Require(FT_Stroker_New(library, &expectedStroker) == 0, "create reference stroker");
    Require(FT_Stroker_New(library, &actualStroker) == 0, "create guest stroker");
    Prepare(expectedStroker);
    Prepare(actualStroker);

    FT_UInt points = 0;
    FT_UInt contours = 0;
    FT_Stroker_GetCounts(expectedStroker, &points, &contours);
    Output expected(points, contours);
    Output actual(points, contours);
    FT_Stroker_Export(expectedStroker, &expected.outline);

    FreeType::GuestVector* guestPoints = reinterpret_cast<FreeType::GuestVector*>(actual.points.data());
    FreeType::GuestOutline guestOutline{0, 0, guestPoints,
        actual.tags.data(), actual.contours.data(), 0};
    sceFreeTypeOtStrokerExport_nid_no_patch(actualStroker, &guestOutline);

    std::fprintf(stderr, "expected points=%d contours=%d actual points=%u contours=%u\n",
                 expected.outline.n_points, expected.outline.n_contours,
                 guestOutline.n_points, guestOutline.n_contours);
    Require(guestOutline.n_points == expected.outline.n_points, "point count matches FreeType");
    Require(guestOutline.n_contours == expected.outline.n_contours, "contour count matches FreeType");
    Require(guestOutline.n_points > 0 && guestOutline.n_contours > 0, "stroke produced geometry");
    for (std::size_t i = 0; i < guestOutline.n_points; ++i) {
        Require(actual.points[i].x == expected.points[i].x && actual.points[i].y == expected.points[i].y,
                "point coordinates match FreeType");
        Require(actual.tags[i] == expected.tags[i], "point tags match FreeType");
    }
    for (std::size_t i = 0; i < guestOutline.n_contours; ++i) {
        Require(actual.contours[i] == expected.contours[i], "contour endpoints match FreeType");
    }

    FT_Stroker_Done(expectedStroker);
    FT_Stroker_Done(actualStroker);
    FT_Done_FreeType(library);
    std::puts("FreeTypeOt stroker export tests passed");
    return 0;
}
