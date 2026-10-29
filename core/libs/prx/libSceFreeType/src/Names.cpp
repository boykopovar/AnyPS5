#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_SFNT_NAMES_H

#include <cstdint>

using namespace FreeType;

extern "C" {

int APS5_VABI FT_Get_Glyph_Name_nid_postfix(GuestFaceRec* face, std::uint32_t glyphIndex, void* buffer, std::uint32_t bufferSize) {
    const GuestFace* owner = FaceOf(face);
    const FT_Face host = owner ? owner->host : nullptr;
    if (!host || !buffer || bufferSize == 0) return FT_Get_Glyph_Name(host, glyphIndex, buffer, bufferSize);
    GuestArena::HostWrite write(buffer, bufferSize);
    if (static_cast<std::int64_t>(glyphIndex) >= host->num_glyphs) {
        static_cast<char*>(buffer)[0] = '\0';
        return FT_Err_Invalid_Glyph_Index;
    }
    return FT_Get_Glyph_Name(host, glyphIndex, buffer, bufferSize);
}

std::uint32_t APS5_VABI FT_Get_Name_Index_nid_postfix(GuestFaceRec* face, const char* glyphName) {
    const GuestFace* owner = FaceOf(face);
    return FT_Get_Name_Index(owner ? owner->host : nullptr, glyphName);
}

const char* APS5_VABI FT_Get_Postscript_Name_nid_postfix(GuestFaceRec* face) {
    const GuestFace* owner = FaceOf(face);
    return FT_Get_Postscript_Name(owner ? owner->host : nullptr);
}

std::uint32_t APS5_VABI FT_Get_Sfnt_Name_Count_nid_postfix(GuestFaceRec* face) {
    const GuestFace* owner = FaceOf(face);
    return FT_Get_Sfnt_Name_Count(owner ? owner->host : nullptr);
}

int APS5_VABI FT_Get_Sfnt_Name_nid_postfix(GuestFaceRec* face, std::uint32_t index, GuestSfntName* name) {
    const GuestFace* owner = FaceOf(face);
    FT_SfntName native{};
    const FT_Error error = FT_Get_Sfnt_Name(owner ? owner->host : nullptr, index, name ? &native : nullptr);
    if (!error) {
        name->platform_id = native.platform_id;
        name->encoding_id = native.encoding_id;
        name->language_id = native.language_id;
        name->name_id = native.name_id;
        name->string = native.string;
        name->string_len = native.string_len;
    }
    return error;
}

}
