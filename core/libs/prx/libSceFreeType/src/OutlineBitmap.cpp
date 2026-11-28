#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_OUTLINE_H

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace FreeType;

namespace {

FT_Pos HostCoordinate(std::int64_t value) {
    if (value < std::numeric_limits<FT_Pos>::min() || value > std::numeric_limits<FT_Pos>::max()) {
        throw std::runtime_error("FT_Outline_Get_Bitmap: an outline coordinate outside the host FT_Pos range is not implemented");
    }
    return static_cast<FT_Pos>(value);
}

}

extern "C" {

int APS5_VABI FreeTypeOutlineGetBitmap_nid_no_patch(GuestLibrary* library, GuestOutline* outline, const GuestBitmap* bitmap) {
    FT_Library host = library ? library->host : nullptr;
    GuestBitmap source{};
    if (!bitmap || !GuestCopyIn_nid_no_patch(&source, bitmap, sizeof(source))) return FT_Outline_Get_Bitmap(host, nullptr, nullptr);
    const FT_Bitmap target{source.rows, source.width, source.pitch, source.buffer, source.num_grays, source.pixel_mode, source.palette_mode, source.palette};
    GuestOutline header{};
    if (!outline || !GuestCopyIn_nid_no_patch(&header, outline, sizeof(header))) return FT_Outline_Get_Bitmap(host, nullptr, &target);
    std::vector<GuestVector> guestPoints(header.n_points);
    std::vector<unsigned char> tags(header.n_points);
    std::vector<std::uint16_t> contours(header.n_contours);
    if (!GuestCopyIn_nid_no_patch(guestPoints.data(), header.points, guestPoints.size() * sizeof(GuestVector)) ||
        !GuestCopyIn_nid_no_patch(tags.data(), header.tags, tags.size()) ||
        !GuestCopyIn_nid_no_patch(contours.data(), header.contours, contours.size() * sizeof(std::uint16_t))) {
        return FT_Outline_Get_Bitmap(host, nullptr, &target);
    }
    std::vector<FT_Vector> points;
    points.reserve(guestPoints.size());
    for (const GuestVector& point : guestPoints) points.push_back({HostCoordinate(point.x), HostCoordinate(point.y)});
    FT_Outline native{header.n_contours, header.n_points, points.data(), tags.data(), contours.data(), header.flags};
    const RasterTargetSpan written = RasterTargetSpan_nid_no_patch(source);
    GuestArena::HostWrite write(written.begin, written.bytes);
    if (written.bytes != 0 && (!write.Open() || !GuestWritable_nid_no_patch(written.begin, written.bytes))) return FT_Err_Invalid_Argument;
    return FT_Outline_Get_Bitmap(host, &native, &target);
}

int APS5_VABI FT_Outline_Get_Bitmap_nid_postfix(GuestLibrary* library, GuestOutline* outline, const GuestBitmap* bitmap) {
    return FreeTypeOutlineGetBitmap_nid_no_patch(library, outline, bitmap);
}

}
