#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_FONT_FORMATS_H

#include <cstdint>

using namespace FreeType;

extern "C" {

const char* APS5_VABI FT_Get_Font_Format_nid_postfix(GuestFaceRec* face) {
    const GuestFace* owner = FaceOf(face);
    return FT_Get_Font_Format(owner ? owner->host : nullptr);
}

std::uint8_t APS5_VABI FT_Face_CheckTrueTypePatents_nid_postfix(GuestFaceRec* face) {
    const GuestFace* owner = FaceOf(face);
    return FT_Face_CheckTrueTypePatents(owner ? owner->host : nullptr);
}

std::uint8_t APS5_VABI FT_Face_SetUnpatentedHinting_nid_postfix(GuestFaceRec* face, std::uint8_t value) {
    const GuestFace* owner = FaceOf(face);
    return FT_Face_SetUnpatentedHinting(owner ? owner->host : nullptr, value);
}

}
