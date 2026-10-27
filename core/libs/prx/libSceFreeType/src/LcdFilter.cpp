#include "prx/libSceFreeType/include/Api.hpp"
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

int APS5_VABI FT_Library_SetLcdFilterWeights_nid_postfix(GuestLibrary* library, unsigned char* weights) {
    FT_Library host = library ? library->host : nullptr;
    if (!weights) return FT_Library_SetLcdFilterWeights(host, nullptr);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(weights, FT_LCD_FILTER_FIVE_TAPS);
    std::array<unsigned char, FT_LCD_FILTER_FIVE_TAPS> copy;
    std::memcpy(copy.data(), weights, copy.size());
    return FT_Library_SetLcdFilterWeights(host, copy.data());
}

}
