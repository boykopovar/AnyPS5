#include "prx/libfreetype/include/Color.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include FT_COLOR_H

namespace {

FT_Face HostFace(const FreeType::GuestFaceRec* face) {
    const auto* owner = face ? static_cast<const FreeType::GuestFace*>(face->internal) : nullptr;
    return owner ? owner->host : nullptr;
}

static_assert(sizeof(FT_Color) == sizeof(LegacyFreeType::GuestColor));

}

extern "C" {

std::int32_t APS5_VABI FT_Palette_Data_Get_nid_postfix(FreeType::GuestFaceRec* face, LegacyFreeType::GuestPaletteData* paletteData) {
    FT_Palette_Data native{};
    const FT_Error error = FT_Palette_Data_Get(HostFace(face), paletteData ? &native : nullptr);
    if (error == FT_Err_Ok) *paletteData = {native.num_palettes, native.palette_name_ids, native.palette_flags, native.num_palette_entries, native.palette_entry_name_ids};
    return error;
}

std::int32_t APS5_VABI FT_Palette_Select_nid_postfix(FreeType::GuestFaceRec* face, std::uint16_t paletteIndex, LegacyFreeType::GuestColor** palette) {
    FT_Color* native = nullptr;
    const FT_Error error = FT_Palette_Select(HostFace(face), paletteIndex, palette ? &native : nullptr);
    if (error == FT_Err_Ok && palette) *palette = reinterpret_cast<LegacyFreeType::GuestColor*>(native);
    return error;
}

std::uint8_t APS5_VABI FT_Get_Color_Glyph_Layer_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t baseGlyph, std::uint32_t* glyphIndex, std::uint32_t* colorIndex, LegacyFreeType::GuestLayerIterator* iterator) {
    if (!iterator) return FT_Get_Color_Glyph_Layer(HostFace(face), baseGlyph, glyphIndex, colorIndex, nullptr);
    FT_LayerIterator native{iterator->num_layers, iterator->layer, iterator->p};
    const FT_Bool found = FT_Get_Color_Glyph_Layer(HostFace(face), baseGlyph, glyphIndex, colorIndex, &native);
    *iterator = {native.num_layers, native.layer, native.p};
    return found;
}

}
