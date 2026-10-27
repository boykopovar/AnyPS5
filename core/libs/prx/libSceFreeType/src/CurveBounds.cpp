#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libc/include/General.hpp"

#include <ft2build.h>
#include FT_BBOX_H

#include <limits>
#include <vector>

namespace {

FT_Pos NativePosition(std::int64_t value) {
    if (value < std::numeric_limits<FT_Pos>::min() || value > std::numeric_limits<FT_Pos>::max())
        NotImplemented_nid_no_patch("FT_Outline_Get_BBox: coordinate outside the native FT_Pos range");
    return static_cast<FT_Pos>(value);
}

}

extern "C" int APS5_VABI FT_Outline_Get_BBox_nid_postfix(FreeType::GuestOutline* outline, FreeType::GuestBBox* boundingBox) {
    if (!boundingBox) return FT_Outline_Get_BBox(nullptr, nullptr);
    FT_BBox result{};
    if (!outline) return FT_Outline_Get_BBox(nullptr, &result);
    FT_Outline native{outline->n_contours, outline->n_points, nullptr, outline->tags, outline->contours, outline->flags};
    std::vector<FT_Vector> points;
    if (outline->n_points != 0 && outline->n_contours != 0) {
        if (!outline->points || !outline->tags || !outline->contours)
            NotImplemented_nid_no_patch("FT_Outline_Get_BBox: missing nonempty outline arrays");
        if (outline->n_contours > outline->n_points)
            NotImplemented_nid_no_patch("FT_Outline_Get_BBox: inconsistent contour bounds");
        std::size_t first = 0;
        for (std::size_t contour = 0; contour < outline->n_contours; ++contour) {
            const std::size_t last = outline->contours[contour];
            if (last < first || last >= outline->n_points)
                NotImplemented_nid_no_patch("FT_Outline_Get_BBox: inconsistent contour bounds");
            first = last + 1;
        }
        if (first != outline->n_points)
            NotImplemented_nid_no_patch("FT_Outline_Get_BBox: inconsistent contour bounds");
        points.reserve(outline->n_points);
        constexpr std::int64_t curveLimit = std::numeric_limits<std::int32_t>::max() / 4;
        bool curved = false;
        bool wide = false;
        for (std::size_t index = 0; index < outline->n_points; ++index) {
            points.push_back({NativePosition(outline->points[index].x), NativePosition(outline->points[index].y)});
            curved |= (outline->tags[index] & 3) != FT_CURVE_TAG_ON;
            wide |= outline->points[index].x < -curveLimit || outline->points[index].x > curveLimit ||
                outline->points[index].y < -curveLimit || outline->points[index].y > curveLimit;
        }
        if (curved && wide)
            NotImplemented_nid_no_patch("FT_Outline_Get_BBox: curved coordinates exceed the portable arithmetic range");
        native.points = points.data();
    }
    const FT_Error error = FT_Outline_Get_BBox(&native, &result);
    if (error == 0) *boundingBox = {result.xMin, result.yMin, result.xMax, result.yMax};
    return error;
}
