#pragma once

#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" FileStream* APS5_VABI _ZSt7_FiopenPKwNSt5_IosbIiE9_OpenmodeEi_nid_postfix(const std::uint16_t* filename, int mode, int protection);
