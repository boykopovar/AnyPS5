#pragma once

#include "prx/libSceFreeTypeOt/include/Stream.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeTypeOt/include/Calc.hpp"
#include "prx/libSceFreeTypeOt/include/Memory.hpp"

extern "C" {

int APS5_VABI sceFreeTypeOtOpenTypeFree_nid_no_patch();
int APS5_VABI sceFreeTypeOtOpenTypeValidate_nid_no_patch();
int APS5_VABI sceFreeTypeOtTrueTypeGXFree_nid_no_patch();
int APS5_VABI sceFreeTypeOtTrueTypeGXValidate_nid_no_patch();

}
