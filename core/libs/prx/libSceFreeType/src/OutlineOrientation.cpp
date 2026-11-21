#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <ft2build.h>
#include FT_OUTLINE_H

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

using namespace FreeType;

namespace {

struct OutlineArrays {
    std::vector<GuestVector> points;
    std::vector<std::uint16_t> contours;
};

OutlineArrays ReadOutlineArrays(const GuestOutline& outline, const char* function) {
    OutlineArrays arrays{std::vector<GuestVector>(outline.n_points), std::vector<std::uint16_t>(outline.n_contours)};
    if (!GuestCopyIn_nid_no_patch(arrays.points.data(), outline.points, arrays.points.size() * sizeof(GuestVector))) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (!GuestCopyIn_nid_no_patch(arrays.contours.data(), outline.contours, arrays.contours.size() * sizeof(std::uint16_t))) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    for (const std::uint16_t last : arrays.contours) {
        if (last >= outline.n_points) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
    return arrays;
}

}

extern "C" {

int APS5_VABI FT_Outline_Get_Orientation_nid_postfix(GuestOutline* outline) {
    if (!outline) return FT_ORIENTATION_TRUETYPE;
    GuestOutline header{};
    if (!GuestCopyIn_nid_no_patch(&header, outline, sizeof(header))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (header.n_points == 0) return FT_ORIENTATION_TRUETYPE;
    const OutlineArrays arrays = ReadOutlineArrays(header, __func__);
    const auto initial = arrays.points[0];
    GuestBBox bounds{initial.x, initial.y, initial.x, initial.y};
    for (unsigned index = 1; index < header.n_points; ++index) {
        const auto point = arrays.points[index];
        bounds.xMin = std::min(bounds.xMin, point.x);
        bounds.yMin = std::min(bounds.yMin, point.y);
        bounds.xMax = std::max(bounds.xMax, point.x);
        bounds.yMax = std::max(bounds.yMax, point.y);
    }
    if (bounds.xMin == bounds.xMax || bounds.yMin == bounds.yMax ||
        bounds.xMin < -0x1000000 || bounds.yMin < -0x1000000 ||
        bounds.xMax > 0x1000000 || bounds.yMax > 0x1000000) return FT_ORIENTATION_NONE;
    const auto xMagnitude = static_cast<std::uint32_t>((bounds.xMin < 0 ? -bounds.xMin : bounds.xMin) |
                                                       (bounds.xMax < 0 ? -bounds.xMax : bounds.xMax));
    const int xShift = std::max(0, static_cast<int>(std::bit_width(xMagnitude)) - 15);
    const int yShift = std::max(0, static_cast<int>(std::bit_width(static_cast<std::uint32_t>(bounds.yMax - bounds.yMin))) - 15);
    std::int64_t area = 0;
    unsigned first = 0;
    for (const unsigned last : arrays.contours) {
        auto previousX = arrays.points[last].x >> xShift;
        auto previousY = arrays.points[last].y >> yShift;
        for (unsigned index = first; index <= last; ++index) {
            const auto x = arrays.points[index].x >> xShift;
            const auto y = arrays.points[index].y >> yShift;
            area += (y - previousY) * (x + previousX);
            previousX = x;
            previousY = y;
        }
        first = last + 1;
    }
    return area > 0 ? FT_ORIENTATION_POSTSCRIPT : area < 0 ? FT_ORIENTATION_TRUETYPE : FT_ORIENTATION_NONE;
}

int APS5_VABI FT_Outline_Copy_nid_postfix(const GuestOutline* source, GuestOutline* target) {
    if (!source || !target) return FT_Err_Invalid_Outline;
    GuestOutline from{};
    GuestOutline to{};
    if (!GuestCopyIn_nid_no_patch(&from, source, sizeof(from)) || !GuestCopyIn_nid_no_patch(&to, target, sizeof(to))) return FT_Err_Invalid_Outline;
    if (from.n_points != to.n_points || from.n_contours != to.n_contours) return FT_Err_Invalid_Argument;
    if (source == target) return FT_Err_Ok;
    std::vector<GuestVector> points(from.n_points);
    std::vector<unsigned char> tags(from.n_points);
    std::vector<std::uint16_t> contours(from.n_contours);
    const std::size_t pointBytes = points.size() * sizeof(GuestVector);
    const std::size_t contourBytes = contours.size() * sizeof(std::uint16_t);
    if (!GuestCopyIn_nid_no_patch(points.data(), from.points, pointBytes) || !GuestCopyIn_nid_no_patch(tags.data(), from.tags, tags.size()) ||
        !GuestCopyIn_nid_no_patch(contours.data(), from.contours, contourBytes) || !GuestWritable_nid_no_patch(to.points, pointBytes) ||
        !GuestWritable_nid_no_patch(to.tags, tags.size()) || !GuestWritable_nid_no_patch(to.contours, contourBytes) ||
        !GuestWritable_nid_no_patch(target, sizeof(to))) return FT_Err_Invalid_Outline;
    to.flags = (from.flags & ~FT_OUTLINE_OWNER) | (to.flags & FT_OUTLINE_OWNER);
    if (!GuestCopyOut_nid_no_patch(to.points, points.data(), pointBytes) || !GuestCopyOut_nid_no_patch(to.tags, tags.data(), tags.size()) ||
        !GuestCopyOut_nid_no_patch(to.contours, contours.data(), contourBytes) || !GuestCopyOut_nid_no_patch(target, &to, sizeof(to))) return FT_Err_Invalid_Outline;
    return FT_Err_Ok;
}

}
