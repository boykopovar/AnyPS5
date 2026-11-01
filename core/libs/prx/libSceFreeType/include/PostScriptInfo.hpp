#ifndef CORE_LIBS_PRX_LIBSCEFREETYPE_INCLUDE_POSTSCRIPTINFO_HPP
#define CORE_LIBS_PRX_LIBSCEFREETYPE_INCLUDE_POSTSCRIPTINFO_HPP

#include "prx/libSceFreeType/include/GuestFreeType.hpp"

namespace FreeType {

struct GuestPSFontInfo {
    char* version;
    char* notice;
    char* fullName;
    char* familyName;
    char* weight;
    std::int64_t italicAngle;
    std::uint8_t isFixedPitch;
    std::int16_t underlinePosition;
    std::uint16_t underlineThickness;
};

static_assert(sizeof(GuestPSFontInfo) == 56 && alignof(GuestPSFontInfo) == 8);
static_assert(offsetof(GuestPSFontInfo, version) == 0 && offsetof(GuestPSFontInfo, notice) == 8);
static_assert(offsetof(GuestPSFontInfo, fullName) == 16 && offsetof(GuestPSFontInfo, familyName) == 24);
static_assert(offsetof(GuestPSFontInfo, weight) == 32 && offsetof(GuestPSFontInfo, italicAngle) == 40);
static_assert(offsetof(GuestPSFontInfo, isFixedPitch) == 48 && offsetof(GuestPSFontInfo, underlinePosition) == 50);
static_assert(offsetof(GuestPSFontInfo, underlineThickness) == 52);

}

#endif
