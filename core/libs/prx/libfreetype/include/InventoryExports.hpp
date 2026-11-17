#pragma once

#include "SceTypes.hpp"

namespace FreeType { struct GuestLibrary; }

extern "C" {

void APS5_VABI FT_Set_Default_Properties_nid_postfix(FreeType::GuestLibrary* library);

}
