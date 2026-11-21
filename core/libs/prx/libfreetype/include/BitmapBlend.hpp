#pragma once

#include "SceTypes.hpp"
#include "prx/libSceFreeType/include/GuestFreeType.hpp"

#include <cstdint>

namespace LegacyFreeType {

struct Color {
    std::uint8_t blue;
    std::uint8_t green;
    std::uint8_t red;
    std::uint8_t alpha;
};

static_assert(sizeof(Color) == 4);

}

extern "C" {

int APS5_VABI freetypeLegacyBitmapBlend_nid_no_patch(FreeType::GuestLibrary* library, const FreeType::GuestBitmap* source, FreeType::GuestVector sourceOffset,
                                                     FreeType::GuestBitmap* target, FreeType::GuestVector* targetOffset, LegacyFreeType::Color color);

}
