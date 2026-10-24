#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_LCD_FILTER_H

#include <array>
#include <cstring>

using namespace FreeType;

extern "C" {

int APS5_VABI FT_Library_SetLcdFilter_nid_postfix(GuestLibrary* library, std::int32_t filter) {
    return FT_Library_SetLcdFilter(library ? library->host : nullptr, static_cast<FT_LcdFilter>(filter));
}

}
