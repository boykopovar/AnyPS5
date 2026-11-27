#include "prx/libfreetype/include/Color.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include FT_COLOR_H
#include "prx/libc/include/General.hpp"

namespace {

FT_Face HostFace(const FreeType::GuestFaceRec* face) {
    const auto* owner = face ? static_cast<const FreeType::GuestFace*>(face->internal) : nullptr;
    return owner ? owner->host : nullptr;
}

static_assert(sizeof(FT_Color) == sizeof(LegacyFreeType::GuestColor));

template <typename T>
bool WriteOut(T* destination, const T& value) {
    return GuestCopyOut_nid_no_patch(destination, &value, sizeof(T));
}

}

extern "C" {

std::int32_t APS5_VABI FT_Palette_Data_Get_nid_postfix(FreeType::GuestFaceRec* face, LegacyFreeType::GuestPaletteData* paletteData) {
    FT_Palette_Data native{};
    const FT_Error error = FT_Palette_Data_Get(HostFace(face), paletteData ? &native : nullptr);
    if (error != FT_Err_Ok) return error;
    const LegacyFreeType::GuestPaletteData data{native.num_palettes, native.palette_name_ids, native.palette_flags, native.num_palette_entries, native.palette_entry_name_ids};
    return WriteOut(paletteData, data) ? error : FT_Err_Invalid_Argument;
}

std::int32_t APS5_VABI FT_Palette_Select_nid_postfix(FreeType::GuestFaceRec* face, std::uint16_t paletteIndex, LegacyFreeType::GuestColor** palette) {
    FT_Color* native = nullptr;
    const FT_Error error = FT_Palette_Select(HostFace(face), paletteIndex, palette ? &native : nullptr);
    if (error == FT_Err_Ok && palette && !WriteOut(palette, reinterpret_cast<LegacyFreeType::GuestColor*>(native))) return FT_Err_Invalid_Argument;
    return error;
}

std::uint8_t APS5_VABI FT_Get_Color_Glyph_Layer_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t baseGlyph, std::uint32_t* glyphIndex, std::uint32_t* colorIndex, LegacyFreeType::GuestLayerIterator* iterator) {
    if (!iterator) {
        if (glyphIndex && !GuestWritable_nid_no_patch(glyphIndex, sizeof(*glyphIndex))) return 0;
        if (colorIndex && !GuestWritable_nid_no_patch(colorIndex, sizeof(*colorIndex))) return 0;
        std::uint32_t nativeGlyph = 0;
        std::uint32_t nativeColor = 0;
        const FT_Bool found = FT_Get_Color_Glyph_Layer(HostFace(face), baseGlyph, glyphIndex ? &nativeGlyph : nullptr, colorIndex ? &nativeColor : nullptr, nullptr);
        if (found) {
            if (glyphIndex) WriteOut(glyphIndex, nativeGlyph);
            if (colorIndex) WriteOut(colorIndex, nativeColor);
        }
        return found;
    }
    if (glyphIndex && !GuestWritable_nid_no_patch(glyphIndex, sizeof(*glyphIndex))) return 0;
    if (colorIndex && !GuestWritable_nid_no_patch(colorIndex, sizeof(*colorIndex))) return 0;
    LegacyFreeType::GuestLayerIterator localIterator{};
    if (!GuestCopyIn_nid_no_patch(&localIterator, iterator, sizeof(localIterator)) || !GuestWritable_nid_no_patch(iterator, sizeof(*iterator))) return 0;
    FT_LayerIterator native{localIterator.num_layers, localIterator.layer, localIterator.p};
    constexpr std::uint32_t unwritten = 0xffffffffu;
    std::uint32_t nativeGlyph = unwritten;
    std::uint32_t nativeColor = unwritten;
    const FT_Bool found = FT_Get_Color_Glyph_Layer(HostFace(face), baseGlyph, glyphIndex ? &nativeGlyph : nullptr, colorIndex ? &nativeColor : nullptr, &native);
    localIterator = {native.num_layers, native.layer, native.p};
    WriteOut(iterator, localIterator);
    if (glyphIndex && nativeGlyph != unwritten) WriteOut(glyphIndex, nativeGlyph);
    if (colorIndex && nativeColor != unwritten) WriteOut(colorIndex, nativeColor);
    return found;
}

}
