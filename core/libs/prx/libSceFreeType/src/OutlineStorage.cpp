#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_OUTLINE_H

#include <cerrno>
#include <cstdint>
#include <cstdlib>

using namespace FreeType;

namespace {

bool ReadOutline(GuestOutline& copy, const GuestOutline* outline) {
    const int savedErrno = errno;
    const bool read = GuestCopyIn_nid_no_patch(&copy, outline, sizeof(copy)) && GuestWritable_nid_no_patch(const_cast<GuestOutline*>(outline), sizeof(copy));
    errno = savedErrno;
    return read;
}

bool WriteOutline(GuestOutline* outline, const GuestOutline& value) {
    const int savedErrno = errno;
    const bool written = GuestCopyOut_nid_no_patch(outline, &value, sizeof(value));
    errno = savedErrno;
    return written;
}

void ReleaseOwned(const GuestOutline& outline) {
    if (outline.flags & FT_OUTLINE_OWNER) {
        std::free(outline.points);
        std::free(outline.tags);
        std::free(outline.contours);
    }
}

}

extern "C" {

int APS5_VABI FT_Outline_Done_nid_postfix(GuestLibrary* library, GuestOutline* outline) {
    if (!library) return FT_Err_Invalid_Library_Handle;
    if (!outline) return FT_Err_Invalid_Outline;
    if (!library->host) return FT_Err_Invalid_Argument;
    GuestOutline current;
    if (!ReadOutline(current, outline)) return FT_Err_Invalid_Outline;
    ReleaseOwned(current);
    WriteOutline(outline, GuestOutline{});
    return 0;
}

int APS5_VABI FT_Outline_New_nid_postfix(GuestLibrary* library, std::uint32_t numPoints, std::int32_t numContours, GuestOutline* outline) {
    if (!library) return FT_Err_Invalid_Library_Handle;
    if (!outline) return FT_Err_Invalid_Argument;
    if (!library->host) return FT_Err_Invalid_Argument;
    if (!WriteOutline(outline, GuestOutline{})) return FT_Err_Invalid_Argument;
    if (numContours < 0 || static_cast<std::uint32_t>(numContours) > numPoints) return FT_Err_Invalid_Argument;
    if (numPoints > FT_OUTLINE_POINTS_MAX) return FT_Err_Array_Too_Large;
    GuestOutline created{};
    if (numPoints != 0) {
        created.points = static_cast<GuestVector*>(std::calloc(numPoints, sizeof(GuestVector)));
        created.tags = static_cast<unsigned char*>(std::calloc(numPoints, sizeof(unsigned char)));
    }
    if (numContours != 0) created.contours = static_cast<std::uint16_t*>(std::calloc(static_cast<std::uint32_t>(numContours), sizeof(std::uint16_t)));
    const bool failed = numPoints != 0 && (!created.points || !created.tags) || numContours != 0 && !created.contours;
    created.flags |= FT_OUTLINE_OWNER;
    if (failed) {
        ReleaseOwned(created);
        return FT_Err_Out_Of_Memory;
    }
    created.n_points = static_cast<std::uint16_t>(numPoints);
    created.n_contours = static_cast<std::uint16_t>(numContours);
    if (!WriteOutline(outline, created)) {
        ReleaseOwned(created);
        return FT_Err_Invalid_Argument;
    }
    return 0;
}

}
