#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <ft2build.h>
#include FT_STROKER_H

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr bool SharedPoints = sizeof(FT_Pos) == sizeof(std::int64_t);

static_assert(!SharedPoints || (sizeof(FT_Vector) == sizeof(FreeType::GuestVector) &&
                                offsetof(FT_Vector, x) == offsetof(FreeType::GuestVector, x) &&
                                offsetof(FT_Vector, y) == offsetof(FreeType::GuestVector, y)));

}

void FreeTypeOtStrokerExport(void* stroker, FreeType::GuestOutline* outline) {
    if (!stroker || !outline) return;
    if constexpr (SharedPoints) {
        FT_Outline native{outline->n_contours, outline->n_points,
                          reinterpret_cast<FT_Vector*>(outline->points), outline->tags,
                          outline->contours, outline->flags};
        FT_Stroker_Export(static_cast<FT_Stroker>(stroker), &native);
        outline->n_contours = static_cast<std::uint16_t>(native.n_contours);
        outline->n_points = static_cast<std::uint16_t>(native.n_points);
    } else {
        FT_UInt addedPoints = 0;
        FT_UInt addedContours = 0;
        if (FT_Stroker_GetCounts(static_cast<FT_Stroker>(stroker), &addedPoints, &addedContours)) return;

        const auto pointCount = static_cast<std::size_t>(outline->n_points) + addedPoints;
        const auto contourCount = static_cast<std::size_t>(outline->n_contours) + addedContours;
        if (pointCount > std::numeric_limits<FT_UShort>::max() || contourCount > std::numeric_limits<FT_UShort>::max())
            throw std::length_error("FreeType stroker outline exceeds FT_Outline limits");
        if ((pointCount && (!outline->points || !outline->tags)) || (contourCount && !outline->contours))
            throw std::invalid_argument("FreeType stroker outline has null output storage");

        std::vector<FT_Vector> points(pointCount);
        std::vector<FT_Byte> tags(pointCount);
        std::vector<FT_UShort> contours(contourCount);
        for (FT_UInt i = 0; i < outline->n_points; ++i) {
            const auto& point = outline->points[i];
            if (point.x < std::numeric_limits<FT_Pos>::min() || point.x > std::numeric_limits<FT_Pos>::max() ||
                point.y < std::numeric_limits<FT_Pos>::min() || point.y > std::numeric_limits<FT_Pos>::max())
                throw std::out_of_range("existing guest outline point does not fit FT_Pos");
            points[i] = {static_cast<FT_Pos>(point.x), static_cast<FT_Pos>(point.y)};
            tags[i] = outline->tags[i];
        }
        for (FT_UInt i = 0; i < outline->n_contours; ++i)
            contours[i] = static_cast<FT_UShort>(outline->contours[i]);

        FT_Outline native{outline->n_contours, outline->n_points, points.data(), tags.data(),
                          contours.data(), outline->flags};
        FT_Stroker_Export(static_cast<FT_Stroker>(stroker), &native);

        for (FT_UInt i = outline->n_points; i < native.n_points; ++i) {
            outline->points[i].x = static_cast<std::int64_t>(points[i].x);
            outline->points[i].y = static_cast<std::int64_t>(points[i].y);
            outline->tags[i] = tags[i];
        }
        for (FT_UInt i = outline->n_contours; i < native.n_contours; ++i)
            outline->contours[i] = contours[i];
        outline->n_contours = static_cast<std::uint16_t>(native.n_contours);
        outline->n_points = static_cast<std::uint16_t>(native.n_points);
    }
}
