#pragma once

#include "SceTypes.hpp"

namespace FreeType { struct GuestLibrary; }

extern "C" {

int APS5_VABI libfreetypeGetVarAxisFlags_nid_no_patch();
void APS5_VABI FT_Set_Default_Properties_nid_postfix(FreeType::GuestLibrary* library);

}
