#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <ft2build.h>
#include FT_OUTLINE_H

#include <algorithm>
#include <bit>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <vector>

using namespace FreeType;

namespace {

bool CopyInPreservingErrno(void* destination, const void* source, std::size_t bytes) {
    const int saved = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = saved;
    return ok;
}

GuestOutline ReadGuestOutline(const GuestOutline* outline, const char* func) {
    GuestOutline copy{};
    if (!CopyInPreservingErrno(&copy, outline, sizeof(copy))) NotImplementedAt_nid_no_patch(func, __FILE__, __LINE__);
    return copy;
}

std::int32_t* FlagsOf(GuestOutline* outline) {
    return &outline->flags;
}

template <typename Value>
std::vector<Value> ReadGuestArray(const Value* source, std::size_t count, const char* func) {
    std::vector<Value> values(count);
    if (!CopyInPreservingErrno(values.data(), source, count * sizeof(Value))) NotImplementedAt_nid_no_patch(func, __FILE__, __LINE__);
    return values;
}

template <typename Value>
void WriteGuestArray(Value* destination, const std::vector<Value>& values, const char* func) {
    if (!GuestCopyOut_nid_no_patch(destination, values.data(), values.size() * sizeof(Value))) NotImplementedAt_nid_no_patch(func, __FILE__, __LINE__);
}

}

extern "C" {

int APS5_VABI FT_Outline_Check_nid_postfix(GuestOutline* outline) {
    if (!outline) return FT_Outline_Check(nullptr);
    const GuestOutline guest = ReadGuestOutline(outline, __func__);
    std::vector<std::uint16_t> contours;
    if (guest.n_points && guest.n_contours) contours = ReadGuestArray(guest.contours, guest.n_contours, __func__);
    FT_Outline native{guest.n_contours, guest.n_points, nullptr, nullptr, contours.data(), guest.flags};
    return FT_Outline_Check(&native);
}

void APS5_VABI FT_Outline_Get_CBox_nid_postfix(const GuestOutline* outline, GuestBBox* box) {
    if (!outline || !box) return;
    const GuestOutline guest = ReadGuestOutline(outline, __func__);
    const std::vector<GuestVector> points = ReadGuestArray(guest.points, guest.n_points, __func__);
    GuestBBox bounds{};
    if (!points.empty()) {
        const GuestVector& first = points[0];
        bounds = {first.x, first.y, first.x, first.y};
        for (std::size_t index = 1; index < points.size(); ++index) {
            const GuestVector& point = points[index];
            bounds.xMin = std::min(bounds.xMin, point.x);
            bounds.yMin = std::min(bounds.yMin, point.y);
            bounds.xMax = std::max(bounds.xMax, point.x);
            bounds.yMax = std::max(bounds.yMax, point.y);
        }
    }
    if (!GuestCopyOut_nid_no_patch(box, &bounds, sizeof(bounds))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

void APS5_VABI FT_Outline_Translate_nid_postfix(const GuestOutline* outline, std::int64_t xOffset, std::int64_t yOffset) {
    if (!outline) return;
    const GuestOutline guest = ReadGuestOutline(outline, __func__);
    std::vector<GuestVector> points = ReadGuestArray(guest.points, guest.n_points, __func__);
    for (GuestVector& point : points) {
        point.x = std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(point.x) + static_cast<std::uint64_t>(xOffset));
        point.y = std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(point.y) + static_cast<std::uint64_t>(yOffset));
    }
    WriteGuestArray(guest.points, points, __func__);
}

void APS5_VABI FT_Outline_Reverse_nid_postfix(GuestOutline* outline) {
    if (!outline) return;
    const GuestOutline guest = ReadGuestOutline(outline, __func__);
    const std::vector<std::uint16_t> contours = ReadGuestArray(guest.contours, guest.n_contours, __func__);
    std::size_t extent = 0;
    std::size_t first = 1;
    for (const std::size_t last : contours) {
        if (first < last) extent = std::max(extent, last + 1);
        first = last + 2;
    }
    std::vector<GuestVector> points = ReadGuestArray(guest.points, extent, __func__);
    std::vector<unsigned char> tags = ReadGuestArray(guest.tags, extent, __func__);
    first = 1;
    for (const std::size_t last : contours) {
        for (std::size_t left = first, right = last; left < right; ++left, --right) {
            std::swap(points[left], points[right]);
            std::swap(tags[left], tags[right]);
        }
        first = last + 2;
    }
    const std::int32_t flags = guest.flags ^ FT_OUTLINE_REVERSE_FILL;
    if (!GuestWritable_nid_no_patch(guest.points, points.size() * sizeof(GuestVector)) || !GuestWritable_nid_no_patch(guest.tags, tags.size()) ||
        !GuestWritable_nid_no_patch(FlagsOf(outline), sizeof(flags))) {
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    WriteGuestArray(guest.points, points, __func__);
    WriteGuestArray(guest.tags, tags, __func__);
    if (!GuestCopyOut_nid_no_patch(FlagsOf(outline), &flags, sizeof(flags))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

}
