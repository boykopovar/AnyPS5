#pragma once

#include "prx/libSceFreeType/include/GuestFreeType.hpp"

extern "C" {

int APS5_VABI FT_Attach_Stream_nid_postfix(FreeType::GuestFaceRec* face, const FreeType::GuestOpenArgs* args);

}
