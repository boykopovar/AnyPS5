#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_FONT_FORMATS_H

#include <cstdint>

using namespace FreeType;

extern "C" {

const char* APS5_VABI FT_Get_X11_Font_Format_nid_postfix(GuestFaceRec* face) {
    const GuestFace* owner = FaceOf(face);
    return FT_Get_X11_Font_Format(owner ? owner->host : nullptr);
}

std::uint16_t APS5_VABI FT_Get_FSType_Flags_nid_postfix(GuestFaceRec* face) {
    const GuestFace* owner = FaceOf(face);
    return FT_Get_FSType_Flags(owner ? owner->host : nullptr);
}

}
