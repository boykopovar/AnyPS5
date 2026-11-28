#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_LCD_FILTER_H

#include <array>
#include <cerrno>
#include <cstdint>

using namespace FreeType;

namespace {

FT_Library ReadGuestLibrary(GuestLibrary* library, const char* func) {
    if (!library) return nullptr;
    FT_Library host = nullptr;
    if (reinterpret_cast<std::uintptr_t>(library) % alignof(GuestLibrary) != 0 || !GuestCopyIn_nid_no_patch(&host, &library->host, sizeof(host))) NotImplemented_nid_no_patch(func);
    return host;
}

}

extern "C" {

int APS5_VABI FT_Library_SetLcdFilter_nid_postfix(GuestLibrary* library, std::int32_t filter) {
    return FT_Library_SetLcdFilter(ReadGuestLibrary(library, __func__), static_cast<FT_LcdFilter>(filter));
}

int APS5_VABI FT_Library_SetLcdFilterWeights_nid_postfix(GuestLibrary* library, unsigned char* weights) {
    const FT_Library host = ReadGuestLibrary(library, __func__);
    if (!weights) return FT_Library_SetLcdFilterWeights(host, nullptr);
    std::array<unsigned char, FT_LCD_FILTER_FIVE_TAPS> copy{};
    if (!GuestCopyIn_nid_no_patch(copy.data(), weights, copy.size())) NotImplemented_nid_no_patch(__func__);
    return FT_Library_SetLcdFilterWeights(host, copy.data());
}

}
