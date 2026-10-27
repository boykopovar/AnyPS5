#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

std::int32_t APS5_VABI FT_SqrtFixed_nid_postfix(std::int32_t value);
int APS5_VABI sceFreeTypeOtSqrtFixed_nid_no_patch();

}
