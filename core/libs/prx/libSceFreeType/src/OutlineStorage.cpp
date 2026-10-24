#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_OUTLINE_H

#include <cstdint>
#include <cstdlib>

using namespace FreeType;

extern "C" {

int APS5_VABI FT_Outline_Done_nid_postfix(GuestLibrary* library, GuestOutline* outline) {
    if (!library) return FT_Err_Invalid_Library_Handle;
    if (!outline) return FT_Err_Invalid_Outline;
    if (outline->flags & FT_OUTLINE_OWNER) {
        std::free(outline->points);
        std::free(outline->tags);
        std::free(outline->contours);
    }
    *outline = {};
    return 0;
}

int APS5_VABI FT_Outline_New_nid_postfix(GuestLibrary* library, std::uint32_t numPoints, std::int32_t numContours, GuestOutline* outline) {
    if (!library) return FT_Err_Invalid_Library_Handle;
    if (!outline) return FT_Err_Invalid_Argument;
    *outline = {};
    if (numContours < 0 || static_cast<std::uint32_t>(numContours) > numPoints) return FT_Err_Invalid_Argument;
    if (numPoints > FT_OUTLINE_POINTS_MAX) return FT_Err_Array_Too_Large;
    if (numPoints != 0) {
        outline->points = static_cast<GuestVector*>(std::calloc(numPoints, sizeof(GuestVector)));
        outline->tags = static_cast<unsigned char*>(std::calloc(numPoints, sizeof(unsigned char)));
    }
    if (numContours != 0) outline->contours = static_cast<std::uint16_t*>(std::calloc(static_cast<std::uint32_t>(numContours), sizeof(std::uint16_t)));
    const bool failed = numPoints != 0 && (!outline->points || !outline->tags) || numContours != 0 && !outline->contours;
    outline->flags |= FT_OUTLINE_OWNER;
    if (failed) {
        FT_Outline_Done_nid_postfix(library, outline);
        return FT_Err_Out_Of_Memory;
    }
    outline->n_points = static_cast<std::uint16_t>(numPoints);
    outline->n_contours = static_cast<std::uint16_t>(numContours);
    return 0;
}

}
