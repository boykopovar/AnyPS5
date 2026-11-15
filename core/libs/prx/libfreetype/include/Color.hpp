#pragma once

#include "prx/libfreetype/include/FaceSettings.hpp"

namespace LegacyFreeType {

struct GuestPaletteData {
    std::uint16_t num_palettes;
    const std::uint16_t* palette_name_ids;
    const std::uint16_t* palette_flags;
    std::uint16_t num_palette_entries;
    const std::uint16_t* palette_entry_name_ids;
};

struct GuestLayerIterator {
    std::uint32_t num_layers;
    std::uint32_t layer;
    std::uint8_t* p;
};

static_assert(sizeof(GuestPaletteData) == 40 && sizeof(GuestLayerIterator) == 16);

}

extern "C" {

std::int32_t APS5_VABI FT_Palette_Data_Get_nid_postfix(FreeType::GuestFaceRec* face, LegacyFreeType::GuestPaletteData* paletteData);
std::int32_t APS5_VABI FT_Palette_Select_nid_postfix(FreeType::GuestFaceRec* face, std::uint16_t paletteIndex, LegacyFreeType::GuestColor** palette);
std::uint8_t APS5_VABI FT_Get_Color_Glyph_Layer_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t baseGlyph, std::uint32_t* glyphIndex, std::uint32_t* colorIndex, LegacyFreeType::GuestLayerIterator* iterator);

}
