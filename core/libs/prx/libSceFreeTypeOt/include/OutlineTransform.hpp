#pragma once

#include "prx/libSceFreeType/include/GuestFreeType.hpp"

#include <cstdint>

extern "C" {

void APS5_VABI sceFreeTypeOtOutlineTransform_nid_no_patch(const FreeType::GuestOutline* outline, const FreeType::GuestMatrix* matrix);

}
