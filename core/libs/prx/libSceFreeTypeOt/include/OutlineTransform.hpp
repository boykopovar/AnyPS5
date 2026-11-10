#pragma once

#include "prx/libSceFreeType/include/GuestFreeType.hpp"

#include <cstdint>

extern "C" {

void APS5_VABI sceFreeTypeOtOutlineTransform_nid_no_patch(const FreeType::GuestOutline* outline, const FreeType::GuestMatrix* matrix);
int APS5_VABI sceFreeTypeOtOutlineEmbolden_nid_no_patch(FreeType::GuestOutline* outline, std::int64_t strength);
int APS5_VABI sceFreeTypeOtOutlineEmboldenXY_nid_no_patch(FreeType::GuestOutline* outline, std::int64_t xstrength, std::int64_t ystrength);

}
