#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libSceFreeType/include/PostScriptInfo.hpp"

#include FT_TYPE1_TABLES_H

using namespace FreeType;

extern "C" {

int APS5_VABI FT_Get_PS_Font_Info_nid_postfix(GuestFaceRec* face, GuestPSFontInfo* info) {
    const GuestFace* owner = FaceOf(face);
    PS_FontInfoRec native{};
    const int error = FT_Get_PS_Font_Info(owner ? owner->host : nullptr, info ? &native : nullptr);
    if (error != 0) return error;
    info->version = native.version;
    info->notice = native.notice;
    info->fullName = native.full_name;
    info->familyName = native.family_name;
    info->weight = native.weight;
    info->italicAngle = native.italic_angle;
    info->isFixedPitch = native.is_fixed_pitch;
    info->underlinePosition = native.underline_position;
    info->underlineThickness = native.underline_thickness;
    return 0;
}

int APS5_VABI FT_Has_PS_Glyph_Names_nid_postfix(GuestFaceRec* face) {
    const GuestFace* owner = FaceOf(face);
    return FT_Has_PS_Glyph_Names(owner ? owner->host : nullptr);
}

}
