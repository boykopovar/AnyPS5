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

}
