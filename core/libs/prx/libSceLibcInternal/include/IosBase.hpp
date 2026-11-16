#pragma once

#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

StreamManipulator APS5_VABI _ZSt11setiosflagsNSt5_IosbIiE9_FmtflagsE_nid_postfix(std::int32_t mask);
StreamManipulator APS5_VABI sceLibcInternalResetiosflags_nid_no_patch(std::int32_t mask);
StreamManipulator APS5_VABI sceLibcInternalSetbase_nid_no_patch(std::int32_t base);

}
