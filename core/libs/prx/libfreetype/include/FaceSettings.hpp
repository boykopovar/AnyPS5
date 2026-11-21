#pragma once

#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "SceTypes.hpp"

namespace LegacyFreeType {

struct GuestParameter {
    std::uint64_t tag;
    void* data;
};

struct GuestColor {
    std::uint8_t blue;
    std::uint8_t green;
    std::uint8_t red;
    std::uint8_t alpha;
};

static_assert(sizeof(GuestParameter) == 16 && sizeof(GuestColor) == 4);

}

extern "C" {

std::int32_t APS5_VABI FT_Face_Properties_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t numProperties, LegacyFreeType::GuestParameter* properties);
std::int32_t APS5_VABI FT_Palette_Set_Foreground_Color_nid_postfix(FreeType::GuestFaceRec* face, LegacyFreeType::GuestColor color);

}
