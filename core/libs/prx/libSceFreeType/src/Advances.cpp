#include "prx/libSceFreeType/libSceFreeType.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"

#include FT_ADVANCES_H
#include FT_FONT_FORMATS_H
#include FT_TRUETYPE_TABLES_H

#include <cstdint>
#include <cstring>
#include <vector>

using namespace FreeType;

namespace {

bool FastQueryLoadsGlyph(FT_Face face, std::int32_t flags) {
    const char* format = FT_Get_Font_Format(face);
    if (!format || std::strcmp(format, "CFF") != 0) return false;
    if (!FT_IS_SFNT(face)) return true;
    if (flags & FT_LOAD_VERTICAL_LAYOUT) return !FT_HAS_VERTICAL(face);
    const auto* header = static_cast<const TT_HoriHeader*>(FT_Get_Sfnt_Table(face, ft_sfnt_hhea));
    return !header || header->number_Of_HMetrics == 0;
}

int GetAdvances(GuestFaceRec* face, std::uint32_t start, std::uint32_t count, std::int32_t flags, std::int64_t* advances, bool single) {
    GuestFace* owner = FaceOf(face);
    if (!owner) return FT_Err_Invalid_Face_Handle;
    if (!advances) return FT_Err_Invalid_Argument;
    const auto numGlyphs = static_cast<std::uint32_t>(owner->host->num_glyphs);
    const std::uint32_t end = start + count;
    if (start >= numGlyphs || end < start || end > numGlyphs) return FT_Err_Invalid_Glyph_Index;
    if (count == 0) return 0;

    std::vector<FT_Fixed> values(count);
    if constexpr (sizeof(FT_Fixed) == sizeof(std::int64_t)) {
        std::memcpy(values.data(), advances, values.size() * sizeof(FT_Fixed));
    }
    const auto query = [&](std::int32_t loadFlags) {
        return single ? FT_Get_Advance(owner->host, start, loadFlags, values.data())
                      : FT_Get_Advances(owner->host, start, count, loadFlags, values.data());
    };
    FT_Error error = query(flags | FT_ADVANCE_FLAG_FAST_ONLY);
    bool syncSlot = error != FT_Err_Unimplemented_Feature && FastQueryLoadsGlyph(owner->host, flags);
    if (error == FT_Err_Unimplemented_Feature && !(flags & FT_ADVANCE_FLAG_FAST_ONLY)) {
        error = query(flags);
        syncSlot = true;
    }
    if (syncSlot) SyncSlot(*owner);
    if constexpr (sizeof(FT_Fixed) == sizeof(std::int64_t)) {
        std::memcpy(advances, values.data(), values.size() * sizeof(FT_Fixed));
    } else {
        if (error != 0 && error != FT_Err_Unimplemented_Feature) {
            NotImplemented_nid_no_patch("FreeType advance query: partial output after a native backend error with 32-bit FT_Fixed");
        }
        if (error == 0) {
            for (std::uint32_t index = 0; index < count; ++index) advances[index] = values[index];
        }
    }
    return error;
}

}

extern "C" {

int APS5_VABI FT_Get_Advance_nid_postfix(GuestFaceRec* face, std::uint32_t glyphIndex, std::int32_t flags, std::int64_t* advance) {
    return GetAdvances(face, glyphIndex, 1, flags, advance, true);
}

int APS5_VABI FT_Get_Advances_nid_postfix(GuestFaceRec* face, std::uint32_t start, std::uint32_t count, std::int32_t flags, std::int64_t* advances) {
    return GetAdvances(face, start, count, flags, advances, false);
}

}
