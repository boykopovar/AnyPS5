#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libSceFreeType/include/PostScriptInfo.hpp"
#include "prx/libc/include/General.hpp"

#include FT_TYPE1_TABLES_H

#include <cerrno>

using namespace FreeType;

extern "C" {

int APS5_VABI FT_Get_PS_Font_Info_nid_postfix(GuestFaceRec* face, GuestPSFontInfo* info) {
    const GuestFace* owner = FaceOf(face);
    PS_FontInfoRec native{};
    const int error = FT_Get_PS_Font_Info(owner ? owner->host : nullptr, info ? &native : nullptr);
    if (error != 0) return error;
    GuestPSFontInfo guestInfo{};
    guestInfo.version = native.version;
    guestInfo.notice = native.notice;
    guestInfo.fullName = native.full_name;
    guestInfo.familyName = native.family_name;
    guestInfo.weight = native.weight;
    guestInfo.italicAngle = native.italic_angle;
    guestInfo.isFixedPitch = native.is_fixed_pitch;
    guestInfo.underlinePosition = native.underline_position;
    guestInfo.underlineThickness = native.underline_thickness;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(info, &guestInfo, sizeof(guestInfo));
    errno = savedErrno;
    return copied ? 0 : FT_Err_Invalid_Argument;
}

int APS5_VABI FT_Has_PS_Glyph_Names_nid_postfix(GuestFaceRec* face) {
    const GuestFace* owner = FaceOf(face);
    return FT_Has_PS_Glyph_Names(owner ? owner->host : nullptr);
}

}
